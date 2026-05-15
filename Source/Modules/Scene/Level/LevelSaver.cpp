#include "LevelSaver.h"

#include "LevelAsset.h"
#include "../../Game/World.h"
#include "../../Game/Actor.h"
#include "../../Game/Actors/CameraActor.h"
#include "../../Game/Actors/CubeMapActor.h"
#include "../../Game/Actors/DirectionalLightActor.h"
#include "../../Game/Actors/SpotLightActor.h"
#include "../../Game/Actors/StaticMeshActor.h"
#include "../../Game/Components/SceneComponent.h"
#include "../../Game/Components/MeshComponent.h"
#include "../../Game/MeshData.h"
#include "../../Core/Asset/AssetIdentity.h"
#include "../../Graphics/Material/MaterialInstance.h"

namespace
{
    constexpr const char* MeshReferencePropertyKey = "asset.meshReference";
    constexpr const char* MaterialReferencePropertyKey = "asset.materialReference";
    constexpr const char* MaterialSourcePathPropertyKey = "material.sourcePath";

    void FillTransform(SceneComponent* root, LevelActorDesc& out)
    {
        if (!root)
        {
            return;
        }

        const Transform& t = root->GetRelativeTransform();
        out.position = t.m_Pos;
        out.rotationEuler = { t.m_Rot.Pitch, t.m_Rot.Yaw, t.m_Rot.Roll };
        out.scale = t.m_Scale;
    }

    void WriteFloat3Property(LevelActorDesc& out, const std::string& key, const DirectX::XMFLOAT3& value)
    {
        out.properties[key] =
            std::to_string(value.x) + "," +
            std::to_string(value.y) + "," +
            std::to_string(value.z);
    }

    void WriteFloat4Property(LevelActorDesc& out, const std::string& key, const DirectX::XMFLOAT4& value)
    {
        out.properties[key] =
            std::to_string(value.x) + "," +
            std::to_string(value.y) + "," +
            std::to_string(value.z) + "," +
            std::to_string(value.w);
    }

    void WriteReferenceProperty(LevelActorDesc& out, const char* key, const std::string& value)
    {
        if (!value.empty())
        {
            out.properties[key] = value;
        }
    }
}

LevelAsset LevelSaver::BuildLevelAssetFromWorld(const World& world, const std::string& levelName)
{
    LevelAsset level{};
    level.name = levelName;

    CameraActor* activeCamera = world.GetCameraManager().GetActiveCamera();
    if (activeCamera)
    {
        level.requestedActiveCameraName = activeCamera->GetName();
    }

    for (const std::shared_ptr<Actor>& actorBase : world.GetActors())
    {
        if (!actorBase)
        {
            continue;
        }

        LevelActorDesc desc{};
        desc.name = actorBase->GetName();

        if (auto* actor = dynamic_cast<CameraActor*>(actorBase.get()))
        {
            desc.type = "CameraActor";
            desc.className = desc.type;
            FillTransform(actor->GetRootComponent(), desc);
            desc.cameraFovDegree = actor->GetFovDegree();
            desc.cameraNearZ = actor->GetNearZ();
            desc.cameraFarZ = actor->GetFarZ();
            desc.bSetAsActiveCamera = (activeCamera == actor);
            desc.properties["camera.fov"] = std::to_string(desc.cameraFovDegree);
            desc.properties["camera.nearZ"] = std::to_string(desc.cameraNearZ);
            desc.properties["camera.farZ"] = std::to_string(desc.cameraFarZ);
            desc.properties["camera.active"] = desc.bSetAsActiveCamera ? "1" : "0";
            if (desc.bSetAsActiveCamera)
            {
                level.requestedActiveCameraName = desc.name;
            }
            level.actors.push_back(std::move(desc));
            continue;
        }

        if (auto* actor = dynamic_cast<SpotLightActor*>(actorBase.get()))
        {
            desc.type = "SpotLightActor";
            desc.className = desc.type;
            FillTransform(actor->GetRootComponent(), desc);
            if (actor->GetLightComponent())
            {
                desc.lightColor = actor->GetLightComponent()->color;
                desc.lightIntensity = actor->GetLightComponent()->intensity;
                desc.lightRange = actor->GetLightComponent()->range;
                desc.lightSpotAngleDegrees = DirectX::XMConvertToDegrees(actor->GetLightComponent()->spotAngleRadians);
                WriteFloat3Property(desc, "light.color", desc.lightColor);
                desc.properties["light.intensity"] = std::to_string(desc.lightIntensity);
                desc.properties["light.range"] = std::to_string(desc.lightRange);
                desc.properties["light.spotAngleDegrees"] = std::to_string(desc.lightSpotAngleDegrees);
            }
            level.actors.push_back(std::move(desc));
            continue;
        }

        if (auto* actor = dynamic_cast<DirectionalLightActor*>(actorBase.get()))
        {
            desc.type = "DirectionalLightActor";
            desc.className = desc.type;
            FillTransform(actor->GetRootComponent(), desc);
            if (actor->GetLightComponent())
            {
                desc.lightColor = actor->GetLightComponent()->color;
                desc.lightIntensity = actor->GetLightComponent()->intensity;
                WriteFloat3Property(desc, "light.color", desc.lightColor);
                desc.properties["light.intensity"] = std::to_string(desc.lightIntensity);
            }
            level.actors.push_back(std::move(desc));
            continue;
        }

        if (auto* actor = dynamic_cast<CubeMapActor*>(actorBase.get()))
        {
            desc.type = "CubeMapActor";
            desc.className = desc.type;
            FillTransform(actor->GetRootComponent(), desc);
            level.actors.push_back(std::move(desc));
            continue;
        }

        if (auto* actor = dynamic_cast<StaticMeshActor*>(actorBase.get()))
        {
            desc.type = "StaticMeshActor";
            desc.className = desc.type;
            FillTransform(actor->GetRootComponent(), desc);

            if (const StaticMeshComponent* meshComp = actor->GetMeshComponent())
            {
                if (const MeshAsset* mesh = meshComp->GetMesh())
                {
                    desc.properties["meshSerialization"] = "runtime-mesh-present";
                    const IAssetReferenceable* ref = static_cast<const IAssetReferenceable*>(mesh);
                    if (ref->HasAssetIdentity())
                    {
                        desc.meshReference = ref->GetAssetVirtualPath();
                    }

                    WriteReferenceProperty(desc, MeshReferencePropertyKey, desc.meshReference);
                }

                const std::vector<MaterialInstance*> mats = meshComp->GetMaterialInstances();
                if (!mats.empty() && mats[0] != nullptr)
                {
                    desc.properties["materialSerialization"] = "runtime-material-present";
                    if (const IAssetReferenceable* ref = dynamic_cast<const IAssetReferenceable*>(mats[0]))
                    {
                        if (ref->HasAssetIdentity())
                        {
                            desc.materialReference = ref->GetAssetVirtualPath();
                            WriteReferenceProperty(desc, MaterialReferencePropertyKey, desc.materialReference);
                            if (!ref->GetAssetSourcePath().empty())
                            {
                                desc.properties[MaterialSourcePathPropertyKey] = ref->GetAssetSourcePath();
                            }
                        }
                    }

                    const CBMaterial& cb = mats[0]->GetMaterialConstants();
                    WriteFloat4Property(desc, "material.baseColor", cb.BaseColor);
                    desc.properties["material.roughness"] = std::to_string(cb.Roughness);
                    desc.properties["material.metallic"] = std::to_string(cb.Metallic);
                    WriteFloat3Property(desc, "material.emissive", cb.EmissiveFactor);
                    desc.properties["material.usePackedMR"] = std::to_string(static_cast<int>(cb.PackedMR_GB != 0.0f));
                    desc.properties["material.useNormalMap"] = std::to_string(static_cast<int>(cb.UseNormalMap != 0.0f));
                    desc.properties["material.useGlossMap"] = std::to_string(static_cast<int>(cb.UseGlossMap != 0.0f));

                    const auto& texturePaths = mats[0]->GetTexturePaths();
                    for (size_t i = 0; i < texturePaths.size(); ++i)
                    {
                        if (!texturePaths[i].empty())
                        {
                            desc.properties[std::string("material.texture") + std::to_string(i)] = texturePaths[i];
                        }
                    }

                    WriteReferenceProperty(desc, MaterialReferencePropertyKey, desc.materialReference);
                }
            }

            level.actors.push_back(std::move(desc));
            continue;
        }
    }

    return level;
}

bool LevelSaver::SaveWorldToFile(const World& world, const std::string& path, const std::string& levelName)
{
    const LevelAsset level = BuildLevelAssetFromWorld(world, levelName);
    return LevelAsset::SaveToFile(path, level);
}
