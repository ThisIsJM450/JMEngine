#include "ComponentReflection.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>
#include <imgui.h>

#include "../Core/AppBase.h"
#include "../Core/Asset/AssetManager.h"
#include "../Core/Asset/AssetRegistry.h"
#include "../Core/Asset/AssetIdentity.h"
#include "../Game/Components/ActorComponent.h"
#include "../Game/Components/SceneComponent.h"
#include "../Game/Components/MeshComponent.h"
#include "../Game/Components/DirectionalLightComponent.h"
#include "../Game/Components/PointLightComponent.h"
#include "../Game/Components/SpotLightComponent.h"
#include "../Game/Components/StaticMeshComponent.h"
#include "../Game/Animation/AnimInstance.h"
#include "../Game/Animation/AnimSequence.h"
#include "../Game/Characters/Character.h"
#include "../Game/Components/CharacterMovementComponenet.h"
#include "../Game/MeshData.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshData.h"
#include "../Graphics/Material/MaterialInstance.h"

namespace EditorReflection
{
    namespace
    {
        static std::unordered_map<const SkeletalMeshComponent*, AssetID> g_SelectedAnimAssetByComponent;
        
        enum class ERootMotionApplyPolicy : uint8_t
        {
            UseAssetSetting = 0,
            ForceEnable,
            ForceDisable
        };

        static std::unordered_map<const SkeletalMeshComponent*, ERootMotionApplyPolicy> g_RootMotionPolicyByComponent;
        static std::unordered_map<const SkeletalMeshComponent*, bool> g_ConsumeRootInPoseByComponent;

        template <typename TClass, typename TField>
        constexpr size_t OffsetOf(TField TClass::*member)
        {
            return (size_t)&(((TClass*)0)->*member);
        }

        template <typename TClass>
        PropertyDesc MakeBool(const char* name, bool TClass::*member, const PropertyMeta& meta = {})
        {
            PropertyDesc p{};
            p.Name = name;
            p.Type = EPropertyType::Bool;
            p.Offset = OffsetOf(member);
            p.Meta = meta;
            return p;
        }

        template <typename TClass>
        PropertyDesc MakeFloat(const char* name, float TClass::*member, const PropertyMeta& meta = {})
        {
            PropertyDesc p{};
            p.Name = name;
            p.Type = EPropertyType::Float;
            p.Offset = OffsetOf(member);
            p.Meta = meta;
            return p;
        }

        template <typename TClass>
        PropertyDesc MakeFloat3(const char* name, DirectX::XMFLOAT3 TClass::*member, const PropertyMeta& meta = {})
        {
            PropertyDesc p{};
            p.Name = name;
            p.Type = EPropertyType::Float3;
            p.Offset = OffsetOf(member);
            p.Meta = meta;
            return p;
        }

        template <typename TClass>
        PropertyDesc MakeUInt64(const char* name, uint64_t TClass::*member, const PropertyMeta& meta = {})
        {
            PropertyDesc p{};
            p.Name = name;
            p.Type = EPropertyType::UInt64;
            p.Offset = OffsetOf(member);
            p.Meta = meta;
            return p;
        }

        PropertyDesc MakeCustom(const char* name, CustomPropertyDrawer drawer, const PropertyMeta& meta = {})
        {
            PropertyDesc p{};
            p.Name = name;
            p.Type = EPropertyType::Custom;
            p.Offset = 0;
            p.Meta = meta;
            p.CustomDrawer = drawer;
            return p;
        }

        bool DrawSceneRelativePosition(void* object, const PropertyDesc&)
        {
            SceneComponent* c = static_cast<SceneComponent*>(object);
            Transform& t = c->GetRelativeTransform();
            float pos[3] = { t.m_Pos.x, t.m_Pos.y, t.m_Pos.z };
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat3("##rel_pos", pos, 0.01f))
            {
                t.SetPosition(pos[0], pos[1], pos[2]);
                return true;
            }
            return false;
        }

        bool DrawSceneRelativeRotation(void* object, const PropertyDesc&)
        {
            SceneComponent* c = static_cast<SceneComponent*>(object);
            Transform& t = c->GetRelativeTransform();
            float rot[3] = { t.m_Rot.Pitch, t.m_Rot.Yaw, t.m_Rot.Roll };
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat3("##rel_rot", rot, 0.1f))
            {
                t.SetRotationEuler(rot[0], rot[1], rot[2]);
                return true;
            }
            return false;
        }

        bool DrawSceneRelativeScale(void* object, const PropertyDesc&)
        {
            SceneComponent* c = static_cast<SceneComponent*>(object);
            Transform& t = c->GetRelativeTransform();
            float scl[3] = { t.m_Scale.x, t.m_Scale.y, t.m_Scale.z };
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat3("##rel_scale", scl, 0.01f))
            {
                t.SetScale(scl[0], scl[1], scl[2]);
                return true;
            }
            return false;
        }

        struct AssetPickerState
        {
            std::string filter;
            bool onlyGame = false;
            bool onlyEngine = false;
        };

        std::unordered_map<std::string, AssetPickerState> g_AssetPickerStateByLabel;
        std::unordered_map<int, std::vector<AssetID>> g_RecentAssetIdsByType;

        std::string GetAssetVirtualPathOrNone(const IAssetReferenceable* ref)
        {
            if (!ref || !ref->HasAssetIdentity() || ref->GetAssetVirtualPath().empty())
            {
                return std::string("(None)");
            }
            return ref->GetAssetVirtualPath();
        }

        std::string ToLowerCopy(const std::string& value)
        {
            std::string lowered = value;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lowered;
        }

        bool MatchesPathFilter(const AssetMeta& meta, const AssetPickerState& state, const std::string& lowerFilter)
        {
            if (state.onlyGame && meta.virtualPath.rfind("/Game/", 0) != 0)
            {
                return false;
            }
            if (state.onlyEngine && meta.virtualPath.rfind("/Engine/", 0) != 0)
            {
                return false;
            }
            if (lowerFilter.empty())
            {
                return true;
            }

            std::string haystack = meta.virtualPath;
            const auto nameIt = meta.tags.find("Name");
            if (nameIt != meta.tags.end())
            {
                haystack += " ";
                haystack += nameIt->second;
            }
            if (!meta.sourcePath.empty())
            {
                haystack += " ";
                haystack += meta.sourcePath;
            }
            haystack = ToLowerCopy(haystack);
            return haystack.find(lowerFilter) != std::string::npos;
        }

        void PushRecentSelection(AssetType type, AssetID id)
        {
            if (id == 0)
            {
                return;
            }

            std::vector<AssetID>& recent = g_RecentAssetIdsByType[static_cast<int>(type)];
            recent.erase(std::remove(recent.begin(), recent.end(), id), recent.end());
            recent.insert(recent.begin(), id);
            constexpr size_t kMaxRecentCount = 6;
            if (recent.size() > kMaxRecentCount)
            {
                recent.resize(kMaxRecentCount);
            }
        }

        bool DrawAssetReferenceCombo(const char* label, AssetType type, AssetID currentId, const char* currentPreview, AssetID& outSelectedId)
        {
            if (!GAssetRegistry)
            {
                ImGui::TextUnformatted("AssetRegistry is null.");
                return false;
            }

            AssetQuery q{};
            q.type = type;
            std::vector<AssetID> assets = GAssetRegistry->Query(q);
            std::sort(assets.begin(), assets.end(), [](AssetID lhs, AssetID rhs)
            {
                const AssetMeta* l = GAssetRegistry ? GAssetRegistry->GetMeta(lhs) : nullptr;
                const AssetMeta* r = GAssetRegistry ? GAssetRegistry->GetMeta(rhs) : nullptr;
                const std::string lPath = l ? l->virtualPath : std::string();
                const std::string rPath = r ? r->virtualPath : std::string();
                return lPath < rPath;
            });

            bool changed = false;
            outSelectedId = currentId;

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo(label, currentPreview ? currentPreview : "(None)"))
            {
                AssetPickerState& state = g_AssetPickerStateByLabel[label ? label : ""];
                char filterBuf[160]{};
                std::snprintf(filterBuf, sizeof(filterBuf), "%s", state.filter.c_str());
                const std::string filterId = std::string("##asset_filter_") + (label ? label : "asset");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputTextWithHint(filterId.c_str(), "Search name/path/source...", filterBuf, IM_ARRAYSIZE(filterBuf)))
                {
                    state.filter = filterBuf;
                }
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere(-1);
                }

                ImGui::Checkbox("/Game", &state.onlyGame);
                ImGui::SameLine();
                ImGui::Checkbox("/Engine", &state.onlyEngine);
                if (state.onlyGame && state.onlyEngine)
                {
                    state.onlyEngine = false;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear"))
                {
                    state.filter.clear();
                    state.onlyGame = false;
                    state.onlyEngine = false;
                }

                ImGui::Separator();

                const bool noneSelected = (outSelectedId == 0);
                if (ImGui::Selectable("(None)", noneSelected))
                {
                    outSelectedId = 0;
                    changed = true;
                }
                if (noneSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }

                const std::string lowerFilter = ToLowerCopy(state.filter);
                int visibleCount = 0;

                std::vector<AssetID>& recent = g_RecentAssetIdsByType[static_cast<int>(type)];
                bool drewRecentHeader = false;
                for (AssetID recentId : recent)
                {
                    const AssetMeta* recentMeta = GAssetRegistry->GetMeta(recentId);
                    if (!recentMeta || recentMeta->type != type)
                    {
                        continue;
                    }
                    if (!MatchesPathFilter(*recentMeta, state, lowerFilter))
                    {
                        continue;
                    }

                    if (!drewRecentHeader)
                    {
                        ImGui::SeparatorText("Recent");
                        drewRecentHeader = true;
                    }

                    ++visibleCount;
                    const bool selected = (recentId == outSelectedId);
                    const std::string labelWithTag = recentMeta->virtualPath + "  [recent]";
                    if (ImGui::Selectable(labelWithTag.c_str(), selected))
                    {
                        outSelectedId = recentId;
                        changed = true;
                    }
                }

                ImGui::SeparatorText("All Assets");
                for (AssetID id : assets)
                {
                    const AssetMeta* meta = GAssetRegistry->GetMeta(id);
                    if (!meta || !MatchesPathFilter(*meta, state, lowerFilter))
                    {
                        continue;
                    }

                    ++visibleCount;
                    const bool selected = (id == outSelectedId);
                    if (ImGui::Selectable(meta->virtualPath.c_str(), selected))
                    {
                        outSelectedId = id;
                        changed = true;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                    if (ImGui::IsItemHovered() && !meta->sourcePath.empty())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(meta->virtualPath.c_str());
                        ImGui::Separator();
                        ImGui::TextUnformatted(meta->sourcePath.c_str());
                        ImGui::EndTooltip();
                    }
                }

                ImGui::Separator();
                ImGui::TextDisabled("Visible %d / Total %d", visibleCount, static_cast<int>(assets.size()));
                if (visibleCount == 0)
                {
                    ImGui::TextDisabled("No matching assets.");
                }

                ImGui::EndCombo();
            }

            if (changed)
            {
                PushRecentSelection(type, outSelectedId);
            }

            return changed;
        }

        bool DrawStaticMeshAsset(void* object, const PropertyDesc&)
        {
            StaticMeshComponent* sm = static_cast<StaticMeshComponent*>(object);
            if (!sm || !GAssetManager)
            {
                return false;
            }

            const MeshAsset* currentMesh = sm->GetMesh();
            const IAssetReferenceable* currentRef = dynamic_cast<const IAssetReferenceable*>(currentMesh);
            AssetID selectedId = currentRef ? currentRef->GetAssetID() : 0;
            const std::string preview = GetAssetVirtualPathOrNone(currentRef);
            if (!DrawAssetReferenceCombo("##static_mesh_asset", AssetType::StaticMesh, selectedId, preview.c_str(), selectedId))
            {
                return false;
            }

            if (selectedId == 0)
            {
                sm->SetMesh(nullptr);
                return true;
            }

            const AssetMeta* meta = GAssetRegistry ? GAssetRegistry->GetMeta(selectedId) : nullptr;
            if (!meta)
            {
                return false;
            }

            if (std::shared_ptr<MeshAsset> mesh = GAssetManager->LoadStaticMeshByVirtualPath(meta->virtualPath))
            {
                sm->SetMesh(mesh);
                return true;
            }
            return false;
        }

        bool DrawMeshMaterialAsset(void* object, const PropertyDesc&)
        {
            MeshComponent* meshComponent = static_cast<MeshComponent*>(object);
            if (!meshComponent || !GAssetManager)
            {
                return false;
            }

            bool changed = false;
            const CpuMeshBase* cpuMesh = nullptr;
            if (const StaticMeshComponent* sm = dynamic_cast<const StaticMeshComponent*>(meshComponent))
            {
                cpuMesh = sm->GetMesh();
            }
            else if (const SkeletalMeshComponent* sk = dynamic_cast<const SkeletalMeshComponent*>(meshComponent))
            {
                cpuMesh = sk->GetMesh();
            }

            size_t slotCount = 1;
            std::vector<int> sectionUseCount;
            if (cpuMesh)
            {
                for (const MeshSection& section : cpuMesh->GetSections())
                {
                    slotCount = (std::max)(slotCount, static_cast<size_t>(section.MaterialIndex) + 1ull);
                }
                sectionUseCount.assign(slotCount, 0);
                for (const MeshSection& section : cpuMesh->GetSections())
                {
                    if (section.MaterialIndex < sectionUseCount.size())
                    {
                        ++sectionUseCount[section.MaterialIndex];
                    }
                }
            }

            const std::vector<std::shared_ptr<MaterialInstance>>& currentMaterials = meshComponent->GetMaterialSlots();
            std::vector<std::shared_ptr<MaterialInstance>> assigned;
            assigned.reserve((std::max)(slotCount, currentMaterials.size()));
            for (const std::shared_ptr<MaterialInstance>& material : currentMaterials)
            {
                assigned.push_back(material);
            }
            while (assigned.size() < slotCount)
            {
                assigned.push_back(nullptr);
            }

            if (ImGui::Button("Add Slot##mesh_material_add_slot"))
            {
                assigned.push_back(nullptr);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Trim Trailing Empty##mesh_material_trim"))
            {
                while (assigned.size() > slotCount && !assigned.empty() && !assigned.back())
                {
                    assigned.pop_back();
                    changed = true;
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Slots: %d (Required by mesh: %d)", static_cast<int>(assigned.size()), static_cast<int>(slotCount));

            for (size_t slotIndex = 0; slotIndex < assigned.size(); ++slotIndex)
            {
                const MaterialInstance* currentMaterial = slotIndex < currentMaterials.size() ? currentMaterials[slotIndex].get() : nullptr;
                const IAssetReferenceable* currentRef = dynamic_cast<const IAssetReferenceable*>(currentMaterial);
                AssetID selectedId = currentRef ? currentRef->GetAssetID() : 0;
                const std::string preview = GetAssetVirtualPathOrNone(currentRef);
                const std::string slotLabel = "Material Slot " + std::to_string(slotIndex);
                const std::string comboLabel = "##mesh_material_asset_" + std::to_string(slotIndex);

                ImGui::Separator();
                ImGui::TextUnformatted(slotLabel.c_str());
                if (slotIndex < sectionUseCount.size())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(sections: %d)", sectionUseCount[slotIndex]);
                }

                if (DrawAssetReferenceCombo(comboLabel.c_str(), AssetType::Material, selectedId, preview.c_str(), selectedId))
                {
                    changed = true;
                    if (selectedId == 0)
                    {
                        assigned[slotIndex].reset();
                    }
                    else if (const AssetMeta* meta = GAssetRegistry ? GAssetRegistry->GetMeta(selectedId) : nullptr)
                    {
                        assigned[slotIndex] = GAssetManager->LoadMaterialByVirtualPath(meta->virtualPath);
                    }
                }

                if (selectedId != 0)
                {
                    ImGui::SameLine();
                    const std::string clearLabel = "Clear##mesh_material_clear_" + std::to_string(slotIndex);
                    if (ImGui::SmallButton(clearLabel.c_str()))
                    {
                        assigned[slotIndex].reset();
                        changed = true;
                    }
                }
            }

            if (changed)
            {
                meshComponent->SetMaterial(std::move(assigned));
            }
            return changed;
        }

        bool DrawSkeletalMeshAsset(void* object, const PropertyDesc&)
        {
            SkeletalMeshComponent* sk = static_cast<SkeletalMeshComponent*>(object);
            if (!sk || !GAssetManager)
            {
                return false;
            }

            const SkeletalMeshAsset* currentMesh = sk->GetMesh();
            const IAssetReferenceable* currentRef = dynamic_cast<const IAssetReferenceable*>(currentMesh);
            AssetID selectedId = currentRef ? currentRef->GetAssetID() : 0;
            const std::string preview = GetAssetVirtualPathOrNone(currentRef);
            ImGui::TextDisabled("Tip: search by character name, folder, or source file.");
            if (!DrawAssetReferenceCombo("##skeletal_mesh_asset", AssetType::SkeletalMesh, selectedId, preview.c_str(), selectedId))
            {
                return false;
            }

            if (selectedId == 0)
            {
                sk->SetMesh(nullptr);
                return true;
            }

            const AssetMeta* meta = GAssetRegistry ? GAssetRegistry->GetMeta(selectedId) : nullptr;
            if (!meta)
            {
                return false;
            }

            if (std::shared_ptr<SkeletalMeshAsset> mesh = GAssetManager->LoadSkeletalMeshByVirtualPath(meta->virtualPath))
            {
                sk->SetMesh(mesh);
                return true;
            }
            return false;
        }

        bool DrawSkeletalAnimationAsset(void* object, const PropertyDesc&)
        {
            SkeletalMeshComponent* sk = static_cast<SkeletalMeshComponent*>(object);
            if (!sk)
            {
                return false;
            }

            Skeleton* skeleton = sk->GetSkeleton();
            AnimInstance* animInst = sk->GetAnimInstance();
            if (!skeleton || !animInst)
            {
                ImGui::TextUnformatted("Skeletal mesh/skeleton is not ready.");
                return false;
            }

            if (!GAssetRegistry)
            {
                ImGui::TextUnformatted("AssetRegistry is null.");
                return false;
            }

            AssetQuery q{};
            q.type = AssetType::Animation;
            const std::vector<AssetID> assets = GAssetRegistry->Query(q);
            if (assets.empty())
            {
                ImGui::TextUnformatted("No Animation assets found.");
                return false;
            }

            AssetID& selectedId = g_SelectedAnimAssetByComponent[sk];
            bool validSelected = false;
            for (AssetID id : assets)
            {
                if (id == selectedId)
                {
                    validSelected = true;
                    break;
                }
            }
            if (!validSelected)
            {
                selectedId = assets.front();
            }

            bool changed = false;
            ImGui::PushID(sk);

            auto itPolicy = g_RootMotionPolicyByComponent.find(sk);
            if (itPolicy == g_RootMotionPolicyByComponent.end())
            {
                itPolicy = g_RootMotionPolicyByComponent.emplace(sk, ERootMotionApplyPolicy::UseAssetSetting).first;
            }
            ERootMotionApplyPolicy& rmPolicy = itPolicy->second;

            auto itConsume = g_ConsumeRootInPoseByComponent.find(sk);
            if (itConsume == g_ConsumeRootInPoseByComponent.end())
            {
                itConsume = g_ConsumeRootInPoseByComponent.emplace(sk, true).first;
            }
            bool& bConsumeRootInPose = itConsume->second;

            const AssetMeta* selectedMeta = GAssetRegistry->GetMeta(selectedId);
            const char* preview = selectedMeta ? selectedMeta->virtualPath.c_str() : "(Invalid Asset)";
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##anim_sequence", preview))
            {
                for (AssetID id : assets)
                {
                    const AssetMeta* meta = GAssetRegistry->GetMeta(id);
                    if (!meta) continue;
                    const bool isSelected = (id == selectedId);
                    if (ImGui::Selectable(meta->virtualPath.c_str(), isSelected))
                    {
                        selectedId = id;
                        changed = true;
                    }
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            static const char* kRootMotionPolicyLabels[] = {
                "Use Asset Setting",
                "Force Enable",
                "Force Disable"
            };
            int policyIndex = static_cast<int>(rmPolicy);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("RootMotion Policy##root_motion_policy", &policyIndex, kRootMotionPolicyLabels, IM_ARRAYSIZE(kRootMotionPolicyLabels)))
            {
                rmPolicy = static_cast<ERootMotionApplyPolicy>(policyIndex);
                if (rmPolicy == ERootMotionApplyPolicy::ForceDisable)
                {
                    bConsumeRootInPose = false;
                }
                else if (rmPolicy == ERootMotionApplyPolicy::ForceEnable)
                {
                    bConsumeRootInPose = true;
                }
                changed = true;
            }
            if (ImGui::Checkbox("Consume Root In Pose##consume_root_in_pose", &bConsumeRootInPose))
            {
                changed = true;
            }

            if (ImGui::Button("Apply##animation"))
            {
                if (GAssetManager)
                {
                    std::shared_ptr<AnimSequenceAsset> seq = GAssetManager->LoadAnimSequence(selectedId, *skeleton);
                    if (seq)
                    {
                        for (AnimSection& sec : seq->Sections)
                        {
                            switch (rmPolicy)
                            {
                            case ERootMotionApplyPolicy::UseAssetSetting:
                                break;
                            case ERootMotionApplyPolicy::ForceEnable:
                                sec.bEnableRootMotion = true;
                                break;
                            case ERootMotionApplyPolicy::ForceDisable:
                                sec.bEnableRootMotion = false;
                                break;
                            default:
                                break;
                            }
                        }

                        // ��å�� ���� ��Ÿ�� ������ �����.
                        if (rmPolicy == ERootMotionApplyPolicy::ForceDisable)
                        {
                            bConsumeRootInPose = false;
                        }
                        else if (rmPolicy == ERootMotionApplyPolicy::ForceEnable)
                        {
                            bConsumeRootInPose = true;
                        }

                        animInst->SetConsumeRootInPose(bConsumeRootInPose);
                        animInst->SetSequence(seq);
                        animInst->Play();

                        // Character �̵� ������Ʈ RootMotion ��嵵 �Բ� ����ȭ�Ѵ�.
                        if (Actor* owner = sk->GetOwner())
                        {
                            if (Character* character = dynamic_cast<Character*>(owner))
                            {
                                if (CharacterMovementComponent* move = character->GetMovementComponent())
                                {
                                    if (rmPolicy == ERootMotionApplyPolicy::ForceDisable)
                                    {
                                        move->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
                                    }
                                    else
                                    {
                                        move->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
                                    }
                                }
                            }
                        }

                        changed = true;
                    }
                }
            }

            ImGui::PopID();
            return changed;
        }
        const TypeDesc& GetActorComponentDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(ActorComponent));
                t.DisplayName = "ActorComponent";
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetSceneComponentDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(SceneComponent));
                t.DisplayName = "SceneComponent";
                t.Parent = &GetActorComponentDesc();
                t.Properties = {
                    MakeCustom("Position", &DrawSceneRelativePosition,
                        PropertyMeta{ "Position", "Transform", true, true, false, 0.0f, 0.0f, 0.01f }),
                    MakeCustom("Rotation", &DrawSceneRelativeRotation,
                        PropertyMeta{ "Rotation", "Transform", true, true, false, 0.0f, 0.0f, 0.1f }),
                    MakeCustom("Scale", &DrawSceneRelativeScale,
                        PropertyMeta{ "Scale", "Transform", true, true, false, 0.0f, 0.0f, 0.01f }),
                };
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetMeshComponentDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(MeshComponent));
                t.DisplayName = "MeshComponent";
                t.Parent = &GetSceneComponentDesc();
                t.Properties = {
                    MakeBool<MeshComponent>("CastShadow", &MeshComponent::CastShadow,
                        PropertyMeta{ "Cast Shadow", "Rendering", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeBool<MeshComponent>("ReceiveShadow", &MeshComponent::ReceiveShadow,
                        PropertyMeta{ "Receive Shadow", "Rendering", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeCustom("MaterialAsset", &DrawMeshMaterialAsset,
                        PropertyMeta{ "Material Asset", "Assets", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeUInt64<MeshComponent>("meshID", &MeshComponent::meshID,
                        PropertyMeta{ "Mesh ID", "Rendering", true, false, false, 0.0f, 0.0f, 0.0f }),
                };
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetDirectionalLightDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(DirectionalLightComponent));
                t.DisplayName = "DirectionalLightComponent";
                t.Parent = &GetSceneComponentDesc();
                t.Properties = {
                    MakeFloat3<DirectionalLightComponent>("color", &DirectionalLightComponent::color,
                        PropertyMeta{ "Color", "Light", true, true, true, 0.0f, 10.0f, 0.01f }),
                    MakeFloat<DirectionalLightComponent>("intensity", &DirectionalLightComponent::intensity,
                        PropertyMeta{ "Intensity", "Light", true, true, true, 0.0f, 100000.0f, 0.1f }),
                    MakeBool<DirectionalLightComponent>("castShadow", &DirectionalLightComponent::castShadow,
                        PropertyMeta{ "Cast Shadow", "Shadow", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeFloat<DirectionalLightComponent>("shadowBias", &DirectionalLightComponent::shadowBias,
                        PropertyMeta{ "Shadow Bias", "Shadow", true, true, true, 0.0f, 0.01f, 0.0001f }),
                    MakeFloat<DirectionalLightComponent>("normalBias", &DirectionalLightComponent::normalBias,
                        PropertyMeta{ "Normal Bias", "Shadow", true, true, true, 0.0f, 10.0f, 0.001f }),
                };
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetSpotLightDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(SpotLightComponent));
                t.DisplayName = "SpotLightComponent";
                t.Parent = &GetSceneComponentDesc();
                t.Properties = {
                    MakeFloat<SpotLightComponent>("range", &SpotLightComponent::range,
                        PropertyMeta{ "Range", "Light", true, true, true, 0.0f, 10000.0f, 0.1f }),
                    MakeFloat<SpotLightComponent>("spotAngleRadians", &SpotLightComponent::spotAngleRadians,
                        PropertyMeta{ "Spot Angle (Rad)", "Light", true, true, true, 0.01f, 3.13f, 0.01f }),
                    MakeFloat3<SpotLightComponent>("color", &SpotLightComponent::color,
                        PropertyMeta{ "Color", "Light", true, true, true, 0.0f, 10.0f, 0.01f }),
                    MakeFloat<SpotLightComponent>("intensity", &SpotLightComponent::intensity,
                        PropertyMeta{ "Intensity", "Light", true, true, true, 0.0f, 100000.0f, 0.1f }),
                    MakeBool<SpotLightComponent>("castShadow", &SpotLightComponent::castShadow,
                        PropertyMeta{ "Cast Shadow", "Shadow", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeFloat<SpotLightComponent>("shadowBias", &SpotLightComponent::shadowBias,
                        PropertyMeta{ "Shadow Bias", "Shadow", true, true, true, 0.0f, 0.01f, 0.0001f }),
                    MakeFloat<SpotLightComponent>("normalBias", &SpotLightComponent::normalBias,
                        PropertyMeta{ "Normal Bias", "Shadow", true, true, true, 0.0f, 10.0f, 0.001f }),
                };
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetPointLightDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(PointLightComponent));
                t.DisplayName = "PointLightComponent";
                t.Parent = &GetSceneComponentDesc();
                t.Properties = {
                    MakeFloat<PointLightComponent>("range", &PointLightComponent::range,
                        PropertyMeta{ "Range", "Light", true, true, true, 0.0f, 10000.0f, 0.1f }),
                    MakeFloat3<PointLightComponent>("color", &PointLightComponent::color,
                        PropertyMeta{ "Color", "Light", true, true, true, 0.0f, 10.0f, 0.01f }),
                    MakeFloat<PointLightComponent>("intensity", &PointLightComponent::intensity,
                        PropertyMeta{ "Intensity", "Light", true, true, true, 0.0f, 100000.0f, 0.1f }),
                    MakeBool<PointLightComponent>("castShadow", &PointLightComponent::castShadow,
                        PropertyMeta{ "Cast Shadow", "Shadow", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeFloat<PointLightComponent>("shadowBias", &PointLightComponent::shadowBias,
                        PropertyMeta{ "Shadow Bias", "Shadow", true, true, true, 0.0f, 0.01f, 0.0001f }),
                    MakeFloat<PointLightComponent>("normalBias", &PointLightComponent::normalBias,
                        PropertyMeta{ "Normal Bias", "Shadow", true, true, true, 0.0f, 10.0f, 0.001f }),
                };
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetStaticMeshDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(StaticMeshComponent));
                t.DisplayName = "StaticMeshComponent";
                t.Parent = &GetMeshComponentDesc();
                t.Properties = {
                    MakeCustom("MeshAsset", &DrawStaticMeshAsset,
                        PropertyMeta{ "Mesh Asset", "Assets", true, true, false, 0.0f, 0.0f, 0.0f })
                };
                return t;
            }();
            return desc;
        }

        const TypeDesc& GetSkeletalMeshDesc()
        {
            static const TypeDesc desc = []
            {
                TypeDesc t{};
                t.TypeId = std::type_index(typeid(SkeletalMeshComponent));
                t.DisplayName = "SkeletalMeshComponent";
                t.Parent = &GetMeshComponentDesc();
                t.Properties = {
                    MakeCustom("MeshAsset", &DrawSkeletalMeshAsset,
                        PropertyMeta{ "Mesh Asset", "Assets", true, true, false, 0.0f, 0.0f, 0.0f }),
                    MakeCustom("AnimSequence", &DrawSkeletalAnimationAsset,
                        PropertyMeta{ "Anim Sequence", "Animation", true, true, false, 0.0f, 0.0f, 0.0f })
                };
                return t;
            }();
            return desc;
        }

        const std::unordered_map<std::type_index, const TypeDesc*>& Registry()
        {
            static const std::unordered_map<std::type_index, const TypeDesc*> map = {
                { std::type_index(typeid(ActorComponent)), &GetActorComponentDesc() },
                { std::type_index(typeid(SceneComponent)), &GetSceneComponentDesc() },
                { std::type_index(typeid(MeshComponent)), &GetMeshComponentDesc() },
                { std::type_index(typeid(DirectionalLightComponent)), &GetDirectionalLightDesc() },
                { std::type_index(typeid(PointLightComponent)), &GetPointLightDesc() },
                { std::type_index(typeid(SpotLightComponent)), &GetSpotLightDesc() },
                { std::type_index(typeid(StaticMeshComponent)), &GetStaticMeshDesc() },
                { std::type_index(typeid(SkeletalMeshComponent)), &GetSkeletalMeshDesc() },
            };
            return map;
        }

        bool DrawPropertyValue(void* basePtr, const PropertyDesc& p)
        {
            bool changed = false;
            if (!p.Meta.Editable)
            {
                ImGui::BeginDisabled();
            }

            const char* raw = reinterpret_cast<const char*>(basePtr);
            char* dataPtr = const_cast<char*>(raw + p.Offset);

            switch (p.Type)
            {
            case EPropertyType::Bool:
            {
                bool* v = reinterpret_cast<bool*>(dataPtr);
                changed = ImGui::Checkbox("##value", v);
                break;
            }
            case EPropertyType::Float:
            {
                float* v = reinterpret_cast<float*>(dataPtr);
                const float speed = (p.Meta.Speed > 0.0f) ? p.Meta.Speed : 0.1f;
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (p.Meta.HasRange)
                {
                    changed = ImGui::DragFloat("##value", v, speed, p.Meta.Min, p.Meta.Max);
                }
                else
                {
                    changed = ImGui::DragFloat("##value", v, speed);
                }
                break;
            }
            case EPropertyType::Float3:
            {
                DirectX::XMFLOAT3* v = reinterpret_cast<DirectX::XMFLOAT3*>(dataPtr);
                const float speed = (p.Meta.Speed > 0.0f) ? p.Meta.Speed : 0.1f;
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (p.Meta.HasRange)
                {
                    changed = ImGui::DragFloat3("##value", &v->x, speed, p.Meta.Min, p.Meta.Max);
                }
                else
                {
                    changed = ImGui::DragFloat3("##value", &v->x, speed);
                }
                break;
            }
            case EPropertyType::UInt64:
            {
                uint64_t* v = reinterpret_cast<uint64_t*>(dataPtr);
                ImGui::Text("%llu", static_cast<unsigned long long>(*v));
                changed = false;
                break;
            }
            case EPropertyType::Custom:
            {
                if (p.CustomDrawer)
                {
                    changed = p.CustomDrawer(basePtr, p);
                }
                break;
            }
            default:
                break;
            }

            if (!p.Meta.Editable)
            {
                ImGui::EndDisabled();
            }

            return changed;
        }

        bool DrawSingleProperty(void* basePtr, const PropertyDesc& p)
        {
            if (!p.Meta.Visible)
            {
                return false;
            }

            const char* label = (p.Meta.DisplayName && p.Meta.DisplayName[0] != '\0') ? p.Meta.DisplayName : p.Name;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(p.Name ? p.Name : "prop");
            const bool changed = DrawPropertyValue(basePtr, p);
            ImGui::PopID();
            return changed;
        }

        bool DrawTypeRecursive(void* basePtr, const TypeDesc* type)
        {
            if (!type)
            {
                return false;
            }

            // Keep visible labels unchanged while separating internal ImGui IDs by type scope.
            ImGui::PushID(type->DisplayName ? type->DisplayName : "Type");

            bool changed = false;
            if (type->Parent)
            {
                changed |= DrawTypeRecursive(basePtr, type->Parent);
            }

            std::vector<std::string> categories;
            categories.reserve(type->Properties.size());
            for (const PropertyDesc& p : type->Properties)
            {
                const char* cat = (p.Meta.Category && p.Meta.Category[0] != '\0') ? p.Meta.Category : "Default";
                if (std::find(categories.begin(), categories.end(), cat) == categories.end())
                {
                    categories.emplace_back(cat);
                }
            }

            for (const std::string& category : categories)
            {
                if (ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::BeginTable(("##prop_table_" + category).c_str(), 2,
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX))
                    {
                        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        for (const PropertyDesc& p : type->Properties)
                        {
                            const char* cat = (p.Meta.Category && p.Meta.Category[0] != '\0') ? p.Meta.Category : "Default";
                            if (category == cat)
                            {
                                changed |= DrawSingleProperty(basePtr, p);
                            }
                        }
                        ImGui::EndTable();
                    }
                }
            }
            ImGui::PopID();
            return changed;
        }
    }

    const TypeDesc* FindType(const ActorComponent* component)
    {
        if (!component)
        {
            return nullptr;
        }

        const auto& map = Registry();
        const auto it = map.find(std::type_index(typeid(*component)));
        if (it != map.end())
        {
            return it->second;
        }
        return nullptr;
    }

    bool DrawProperties(ActorComponent* component)
    {
        if (!component)
        {
            return false;
        }

        const TypeDesc* type = FindType(component);
        if (!type)
        {
            return false;
        }

        // Component pointer as stable ID scope: same display text, different internal ID.
        ImGui::PushID(component);
        const bool changed = DrawTypeRecursive(component, type);
        ImGui::PopID();
        return changed;
    }
}


