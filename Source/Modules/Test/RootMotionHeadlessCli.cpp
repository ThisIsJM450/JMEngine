#include "RootMotionHeadlessCli.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../Core/Asset/AssetRegistry.h"
#include "../Game/Animation/AnimInstance.h"
#include "../Game/Animation/AnimSequence.h"
#include "../Game/Characters/Character.h"
#include "../Game/Components/CharacterMovementComponenet.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshData.h"
#include "../Game/World.h"
#include "../Graphics/Importer/FBXImporter.h"
#include "../Graphics/Importer/FBXImporter_Animation.h"
#include "../Graphics/Importer/FBXImportType.h"

namespace
{
    std::string ToLowerAscii(std::string value)
    {
        for (char& c : value)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return value;
    }

    int FindBoneIndexByAnySubstring(const Skeleton& skeleton, const std::vector<std::string>& lowerSubstrings)
    {
        for (int i = 0; i < static_cast<int>(skeleton.Bones.size()); ++i)
        {
            const std::string lowerName = ToLowerAscii(skeleton.Bones[static_cast<size_t>(i)].Name);
            for (const std::string& sub : lowerSubstrings)
            {
                if (!sub.empty() && lowerName.find(sub) != std::string::npos)
                {
                    return i;
                }
            }
        }
        return -1;
    }

    int FindRootBoneIndex(const Skeleton& skeleton)
    {
        const int byName = FindBoneIndexByAnySubstring(skeleton, {"root"});
        if (byName >= 0)
        {
            return byName;
        }

        for (int i = 0; i < static_cast<int>(skeleton.Bones.size()); ++i)
        {
            if (skeleton.Bones[static_cast<size_t>(i)].ParentIndex < 0)
            {
                return i;
            }
        }
        return 0;
    }

    int FindHeadBoneIndex(const Skeleton& skeleton)
    {
        const int head = FindBoneIndexByAnySubstring(skeleton, {"head"});
        if (head >= 0)
        {
            return head;
        }
        return FindBoneIndexByAnySubstring(skeleton, {"neck_01", "neck", "spine_03", "spine2"});
    }

    void ComputeBoneWorldTransforms(
        const Character& actor,
        const SkeletalMeshComponent& sk,
        const AnimInstance& anim,
        const Skeleton& skeleton,
        std::vector<DirectX::XMFLOAT3>& outPos,
        std::vector<DirectX::XMFLOAT4>& outRot,
        std::vector<uint8_t>& outValid)
    {
        using namespace DirectX;

        const size_t boneCount = skeleton.Bones.size();
        outPos.assign(boneCount, XMFLOAT3{});
        outRot.assign(boneCount, XMFLOAT4{0.f, 0.f, 0.f, 1.f});
        outValid.assign(boneCount, 0);

        if (boneCount == 0 || skeleton.Nodes.empty())
        {
            return;
        }
        const std::vector<XMFLOAT4X4>& localPose = anim.GetLocalPose();
        std::vector<XMFLOAT4X4> nodeLocal(skeleton.Nodes.size());
        std::vector<XMFLOAT4X4> nodeGlobal(skeleton.Nodes.size());
        std::vector<uint8_t> visited(skeleton.Nodes.size(), 0);

        for (size_t ni = 0; ni < skeleton.Nodes.size(); ++ni)
        {
            nodeLocal[ni] = skeleton.Nodes[ni].RefLocal;
            XMStoreFloat4x4(&nodeGlobal[ni], XMMatrixIdentity());
        }

        for (size_t bi = 0; bi < skeleton.Bones.size(); ++bi)
        {
            if (bi >= skeleton.BoneToNode.size() || bi >= localPose.size())
            {
                continue;
            }
            const int32_t nodeIdx = skeleton.BoneToNode[bi];
            if (nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= skeleton.Nodes.size())
            {
                continue;
            }
            nodeLocal[static_cast<size_t>(nodeIdx)] = localPose[bi];
        }

        std::function<void(int32_t)> BuildNodeGlobal = [&](int32_t idx)
        {
            if (idx < 0 || static_cast<size_t>(idx) >= skeleton.Nodes.size())
            {
                return;
            }
            if (visited[static_cast<size_t>(idx)])
            {
                return;
            }

            const int32_t parent = skeleton.Nodes[static_cast<size_t>(idx)].Parent;
            if (parent >= 0)
            {
                BuildNodeGlobal(parent);
                const XMMATRIX MLocal = XMLoadFloat4x4(&nodeLocal[static_cast<size_t>(idx)]);
                const XMMATRIX MParent = XMLoadFloat4x4(&nodeGlobal[static_cast<size_t>(parent)]);
                XMStoreFloat4x4(&nodeGlobal[static_cast<size_t>(idx)], MLocal * MParent);
            }
            else
            {
                nodeGlobal[static_cast<size_t>(idx)] = nodeLocal[static_cast<size_t>(idx)];
            }

            visited[static_cast<size_t>(idx)] = 1;
        };

        for (int32_t ni = 0; ni < static_cast<int32_t>(skeleton.Nodes.size()); ++ni)
        {
            BuildNodeGlobal(ni);
        }

        const XMMATRIX MMeshRel = sk.GetRelativeTransform().ToMatrix();
        const XMMATRIX MGlobalInv = XMLoadFloat4x4(&skeleton.GlobalInverse);
        const XMMATRIX MActorRoot = actor.GetRootComponent()->GetRelativeTransform().ToMatrix();
        for (size_t bi = 0; bi < boneCount; ++bi)
        {
            if (bi >= skeleton.BoneToNode.size())
            {
                continue;
            }

            const int32_t boneNode = skeleton.BoneToNode[bi];
            if (boneNode < 0 || static_cast<size_t>(boneNode) >= skeleton.Nodes.size())
            {
                continue;
            }

            const XMMATRIX MBone = XMLoadFloat4x4(&nodeGlobal[static_cast<size_t>(boneNode)]);
            const XMMATRIX MBoneWorld = MBone * MGlobalInv * MMeshRel * MActorRoot;

            XMVECTOR S, R, T;
            if (!XMMatrixDecompose(&S, &R, &T, MBoneWorld))
            {
                continue;
            }

            XMStoreFloat3(&outPos[bi], T);
            XMStoreFloat4(&outRot[bi], XMQuaternionNormalize(R));
            outValid[bi] = 1;
        }
    }

    float QuatDiffDeg(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b)
    {
        using namespace DirectX;
        XMFLOAT4 qa{}, qb{};
        XMStoreFloat4(&qa, XMQuaternionNormalize(a));
        XMStoreFloat4(&qb, XMQuaternionNormalize(b));
        float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
        dot = (std::max)(-1.0f, (std::min)(1.0f, std::fabs(dot)));
        return XMConvertToDegrees(2.0f * std::acos(dot));
    }

    const AssetMeta* FindAssetByVirtualPathOrText(
        const AssetRegistry& reg,
        AssetType type,
        const std::string& exactVirtualPath,
        const std::string& textFallback)
    {
        AssetQuery q{};
        q.type = type;
        q.text = textFallback;
        const std::vector<AssetID> ids = reg.Query(q);
        for (const AssetID id : ids)
        {
            const AssetMeta* meta = reg.GetMeta(id);
            if (!meta)
            {
                continue;
            }
            if (meta->virtualPath == exactVirtualPath)
            {
                return meta;
            }
        }

        if (!ids.empty())
        {
            return reg.GetMeta(ids.front());
        }
        return nullptr;
    }

    int RunRootMotionProbeHeadless();
    int RunRootMotionEquivalenceValidationHeadless();
    int RunRootMotionRuntimeValidationCli(int frameCountOverride);

    int RunRootMotionProbeHeadless()
    {
        const std::string kContentRoot = "D:\\Projects\\JMEngine\\Contents\\";
        const std::string kRegistryPath = "D:\\Projects\\JMEngine\\AssetRegistry.json";
        const std::string kMeshVirtualPath = "/Game/Mesh/passive_marker_man";
        const std::string kAnimVirtualPath = "/Game/Animation/Capoeira";
        const std::string kMeshSearchText = "passive_marker_man";
        const std::string kAnimSearchText = "capoeira";
        const std::string kLogPath = "D:\\Projects\\JMEngine\\JMEngine\\x64\\Debug\\RootMotionProbeHeadless.log";
        std::ofstream logFile(kLogPath, std::ios::out | std::ios::trunc);

        auto Log = [&](const std::string& text)
        {
            std::cout << text << std::endl;
            if (logFile.is_open())
            {
                logFile << text << std::endl;
                logFile.flush();
            }
        };

        Log("[Probe] Headless RootMotion probe started.");
        Log(std::string("[Probe] LogPath: ") + kLogPath);

        AssetRegistry reg;
        reg.SetContentRoot(kContentRoot);
        reg.SetDatabasePath(kRegistryPath);
        if (!reg.LoadFromDisk())
        {
            Log("[Probe][Error] Failed to load AssetRegistry.json");
            return 2;
        }

        const AssetMeta* meshMeta = FindAssetByVirtualPathOrText(reg, AssetType::SkeletalMesh, kMeshVirtualPath, kMeshSearchText);
        const AssetMeta* animMeta = FindAssetByVirtualPathOrText(reg, AssetType::Animation, kAnimVirtualPath, kAnimSearchText);
        if (!meshMeta || !animMeta)
        {
            Log("[Probe][Error] Required assets are missing.");
            Log(std::string("[Probe][Error] meshMeta=") + (meshMeta ? "ok" : "null") + ", animMeta=" + (animMeta ? "ok" : "null"));
            return 3;
        }

        Log(std::string("[Probe] MeshAsset: ") + meshMeta->sourcePath);
        Log(std::string("[Probe] AnimAsset: ") + animMeta->sourcePath);

        ImportOptions opt{};
        opt.bBuildMaterials = false;
        ImportResult importResult{};
        if (!FbxImporter::ImportFBX(meshMeta->sourcePath, opt, importResult) || importResult.Type != EImportedMeshType::Skeletal || !importResult.SkeletalMesh)
        {
            Log("[Probe][Error] Failed to import skeletal mesh.");
            return 4;
        }

        struct ProbeScenario
        {
            std::string Name;
            DirectX::XMFLOAT3 Pos;
            DirectX::XMFLOAT3 RotEulerRad;
            DirectX::XMFLOAT3 Scale;
            DirectX::XMFLOAT3 MeshPos;
            DirectX::XMFLOAT3 MeshRotEulerRad;
            DirectX::XMFLOAT3 MeshScale;
        };

        const std::vector<ProbeScenario> scenarios = {
            {"Identity", {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"PosOffset", {120.f,-45.f,30.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"Yaw90", {0.f,0.f,0.f}, {0.f,1.5707963f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"PitchYawRoll", {-80.f,15.f,60.f}, {0.35f,-1.00f,0.65f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"Pitch1_6", {0.f,0.f,0.f}, {1.60f,0.00f,0.00f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"PosRotScale", {40.f,20.f,-95.f}, {-0.45f,0.80f,-0.25f}, {1.25f,0.85f,1.10f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"MeshOffsetRot", {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {10.f,-20.f,5.f}, {0.2f,1.2f,0.3f}, {1.f,1.f,1.f}},
        };

        float worstScenarioRotMax = 0.0f;
        float worstScenarioRelMax = 0.0f;
        float worstScenarioPosMax = 0.0f;
        std::string worstScenarioName = "None";

        for (const ProbeScenario& scn : scenarios)
        {
            std::ostringstream scnHeader;
            scnHeader << std::fixed << std::setprecision(6)
                      << "[Probe][Scenario] " << scn.Name
                      << " Pos=(" << scn.Pos.x << "," << scn.Pos.y << "," << scn.Pos.z << ")"
                      << " RotRad=(" << scn.RotEulerRad.x << "," << scn.RotEulerRad.y << "," << scn.RotEulerRad.z << ")"
                      << " Scale=(" << scn.Scale.x << "," << scn.Scale.y << "," << scn.Scale.z << ")"
                      << " MeshPos=(" << scn.MeshPos.x << "," << scn.MeshPos.y << "," << scn.MeshPos.z << ")"
                      << " MeshRotRad=(" << scn.MeshRotEulerRad.x << "," << scn.MeshRotEulerRad.y << "," << scn.MeshRotEulerRad.z << ")"
                      << " MeshScale=(" << scn.MeshScale.x << "," << scn.MeshScale.y << "," << scn.MeshScale.z << ")";
            Log(scnHeader.str());

            Character actorOn;
            Character actorOff;
            actorOn.SetName("RM_On");
            actorOff.SetName("RM_Off");
            if (!actorOn.GetRootComponent() || !actorOff.GetRootComponent()) return 5;
            actorOn.GetRootComponent()->GetRelativeTransform().SetPosition(scn.Pos.x, scn.Pos.y, scn.Pos.z);
            actorOn.GetRootComponent()->GetRelativeTransform().SetRotationEuler(scn.RotEulerRad.x, scn.RotEulerRad.y, scn.RotEulerRad.z);
            actorOn.GetRootComponent()->GetRelativeTransform().SetScale(scn.Scale.x, scn.Scale.y, scn.Scale.z);
            actorOff.GetRootComponent()->GetRelativeTransform().SetPosition(scn.Pos.x, scn.Pos.y, scn.Pos.z);
            actorOff.GetRootComponent()->GetRelativeTransform().SetRotationEuler(scn.RotEulerRad.x, scn.RotEulerRad.y, scn.RotEulerRad.z);
            actorOff.GetRootComponent()->GetRelativeTransform().SetScale(scn.Scale.x, scn.Scale.y, scn.Scale.z);

            SkeletalMeshComponent* skOn = actorOn.GetSkeletalComponent();
            SkeletalMeshComponent* skOff = actorOff.GetSkeletalComponent();
            if (!skOn || !skOff) return 6;
            skOn->SetMesh(importResult.SkeletalMesh);
            skOff->SetMesh(importResult.SkeletalMesh);
            skOn->GetRelativeTransform().SetPosition(scn.MeshPos.x, scn.MeshPos.y, scn.MeshPos.z);
            skOn->GetRelativeTransform().SetRotationEuler(scn.MeshRotEulerRad.x, scn.MeshRotEulerRad.y, scn.MeshRotEulerRad.z);
            skOn->GetRelativeTransform().SetScale(scn.MeshScale.x, scn.MeshScale.y, scn.MeshScale.z);
            skOff->GetRelativeTransform().SetPosition(scn.MeshPos.x, scn.MeshPos.y, scn.MeshPos.z);
            skOff->GetRelativeTransform().SetRotationEuler(scn.MeshRotEulerRad.x, scn.MeshRotEulerRad.y, scn.MeshRotEulerRad.z);
            skOff->GetRelativeTransform().SetScale(scn.MeshScale.x, scn.MeshScale.y, scn.MeshScale.z);

            Skeleton* skeleton = skOn->GetSkeleton();
            if (!skeleton) return 7;
            std::shared_ptr<AnimSequenceAsset> seqBase = FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(animMeta->sourcePath, *skeleton);
            if (!seqBase || seqBase->Sections.empty()) return 8;

            std::shared_ptr<AnimSequenceAsset> seqOn = std::make_shared<AnimSequenceAsset>(*seqBase);
            std::shared_ptr<AnimSequenceAsset> seqOff = std::make_shared<AnimSequenceAsset>(*seqBase);
            for (AnimSection& sec : seqOn->Sections) sec.bEnableRootMotion = true;
            for (AnimSection& sec : seqOff->Sections) sec.bEnableRootMotion = false;

            const int rootIdx = FindRootBoneIndex(*skeleton);
            if (!skeleton->Bones.empty())
            {
                std::ostringstream rootSel;
                rootSel << "[Probe][" << scn.Name << "] CompareBoneIndex=" << rootIdx << ", Name=" << skeleton->Bones[static_cast<size_t>(rootIdx)].Name << ", TotalBones=" << skeleton->Bones.size();
                Log(rootSel.str());
            }

            AnimInstance* animOn = skOn->GetAnimInstance();
            AnimInstance* animOff = skOff->GetAnimInstance();
            if (!animOn || !animOff) return 9;
            animOn->SetConsumeRootInPose(true);
            animOn->SetSequence(seqOn);
            animOn->Play("Default", true, 1.0f);
            animOff->SetConsumeRootInPose(false);
            animOff->SetSequence(seqOff);
            animOff->Play("Default", true, 1.0f);

            CharacterMovementComponent* moveOn = actorOn.GetMovementComponent();
            CharacterMovementComponent* moveOff = actorOff.GetMovementComponent();
            if (!moveOn || !moveOff) return 10;
            moveOn->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
            moveOff->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

            const float dt = 1.0f / 60.0f;
            const int totalFrames = static_cast<int>(std::round(20.0f / dt));
            const size_t boneCount = skeleton->Bones.size();
            std::vector<float> maxRotDiffPerBone(boneCount, 0.0f), sumRotDiffPerBone(boneCount, 0.0f), maxRotDiffRelPerBone(boneCount, 0.0f), sumRotDiffRelPerBone(boneCount, 0.0f), maxPosDiffPerBone(boneCount, 0.0f), sumPosDiffPerBone(boneCount, 0.0f);
            std::vector<DirectX::XMFLOAT4> qOnBase(boneCount, DirectX::XMFLOAT4{0.f,0.f,0.f,1.f}), qOffBase(boneCount, DirectX::XMFLOAT4{0.f,0.f,0.f,1.f});
            std::vector<uint8_t> hasBaseQuat(boneCount, 0);
            float globalMaxRotDiffDeg = 0.0f, globalMaxRotDiffDegRel = 0.0f, globalMaxPosDiff = 0.0f;
            double totalRotDiffDeg = 0.0, totalRotDiffDegRel = 0.0, totalPosDiff = 0.0;
            uint64_t totalBoneSamples = 0;
            int sampleCount = 0;

            std::vector<DirectX::XMFLOAT3> onPos, offPos;
            std::vector<DirectX::XMFLOAT4> onRot, offRot;
            std::vector<uint8_t> onValid, offValid;
            for (int frame = 1; frame <= totalFrames; ++frame)
            {
                actorOn.Tick(dt);
                actorOff.Tick(dt);
                ComputeBoneWorldTransforms(actorOn, *skOn, *animOn, *skeleton, onPos, onRot, onValid);
                ComputeBoneWorldTransforms(actorOff, *skOff, *animOff, *skeleton, offPos, offRot, offValid);
                const size_t n = (std::min)(onRot.size(), offRot.size());
                for (size_t bi = 0; bi < n; ++bi)
                {
                    if (bi >= onValid.size() || bi >= offValid.size() || onValid[bi] == 0 || offValid[bi] == 0) continue;
                    const DirectX::XMVECTOR qOn = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&onRot[bi]));
                    const DirectX::XMVECTOR qOff = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&offRot[bi]));
                    const float diffDeg = QuatDiffDeg(qOn, qOff);
                    maxRotDiffPerBone[bi] = (std::max)(maxRotDiffPerBone[bi], diffDeg);
                    sumRotDiffPerBone[bi] += diffDeg;
                    globalMaxRotDiffDeg = (std::max)(globalMaxRotDiffDeg, diffDeg);
                    totalRotDiffDeg += diffDeg;
                    if (!hasBaseQuat[bi])
                    {
                        DirectX::XMStoreFloat4(&qOnBase[bi], qOn);
                        DirectX::XMStoreFloat4(&qOffBase[bi], qOff);
                        hasBaseQuat[bi] = 1;
                    }
                    const DirectX::XMVECTOR qOnRel = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&qOnBase[bi])), qOn);
                    const DirectX::XMVECTOR qOffRel = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&qOffBase[bi])), qOff);
                    const float diffDegRel = QuatDiffDeg(qOnRel, qOffRel);
                    maxRotDiffRelPerBone[bi] = (std::max)(maxRotDiffRelPerBone[bi], diffDegRel);
                    sumRotDiffRelPerBone[bi] += diffDegRel;
                    globalMaxRotDiffDegRel = (std::max)(globalMaxRotDiffDegRel, diffDegRel);
                    totalRotDiffDegRel += diffDegRel;
                    const float dx = onPos[bi].x - offPos[bi].x;
                    const float dy = onPos[bi].y - offPos[bi].y;
                    const float dz = onPos[bi].z - offPos[bi].z;
                    const float posDiff = std::sqrt(dx * dx + dy * dy + dz * dz);
                    maxPosDiffPerBone[bi] = (std::max)(maxPosDiffPerBone[bi], posDiff);
                    sumPosDiffPerBone[bi] += posDiff;
                    globalMaxPosDiff = (std::max)(globalMaxPosDiff, posDiff);
                    totalPosDiff += posDiff;
                    ++totalBoneSamples;
                }
                ++sampleCount;
            }

            const float avgRotDiffDeg = totalBoneSamples > 0 ? static_cast<float>(totalRotDiffDeg / static_cast<double>(totalBoneSamples)) : 0.0f;
            const float avgRotDiffDegRel = totalBoneSamples > 0 ? static_cast<float>(totalRotDiffDegRel / static_cast<double>(totalBoneSamples)) : 0.0f;
            const float avgPosDiff = totalBoneSamples > 0 ? static_cast<float>(totalPosDiff / static_cast<double>(totalBoneSamples)) : 0.0f;
            std::ostringstream result;
            result << std::fixed << std::setprecision(6) << "[Probe][Result][" << scn.Name << "] RotDiffDeg Max=" << globalMaxRotDiffDeg << ", Avg=" << avgRotDiffDeg << " | RelDiffDeg Max=" << globalMaxRotDiffDegRel << ", Avg=" << avgRotDiffDegRel << " | PosDiff Max=" << globalMaxPosDiff << ", Avg=" << avgPosDiff << ", BoneSamples=" << totalBoneSamples << ", FrameSamples=" << sampleCount;
            Log(result.str());

            if (globalMaxRotDiffDeg > worstScenarioRotMax)
            {
                worstScenarioRotMax = globalMaxRotDiffDeg;
                worstScenarioRelMax = globalMaxRotDiffDegRel;
                worstScenarioPosMax = globalMaxPosDiff;
                worstScenarioName = scn.Name;
            }
        }

        std::ostringstream summary;
        summary << std::fixed << std::setprecision(6) << "[Probe][Summary] WorstScenario=" << worstScenarioName << " RotDiffMax=" << worstScenarioRotMax << ", RelDiffMax=" << worstScenarioRelMax << ", PosDiffMax=" << worstScenarioPosMax;
        Log(summary.str());
        Log("[Probe] Completed.");
        return 0;
    }

    int RunRootMotionEquivalenceValidationHeadless()
    {
        const std::string kContentRoot = "D:\\Projects\\JMEngine\\Contents\\";
        const std::string kRegistryPath = "D:\\Projects\\JMEngine\\AssetRegistry.json";
        const std::string kMeshVirtualPath = "/Game/Mesh/passive_marker_man";
        const std::string kAnimVirtualPath = "/Game/Animation/Capoeira";
        const std::string kMeshSearchText = "passive_marker_man";
        const std::string kAnimSearchText = "capoeira";
        const std::string kLogPath = "D:\\Projects\\JMEngine\\JMEngine\\x64\\Debug\\RootMotionEquivalenceValidation.log";
        std::ofstream logFile(kLogPath, std::ios::out | std::ios::trunc);
        auto Log = [&](const std::string& text) { std::cout << text << std::endl; if (logFile.is_open()) { logFile << text << std::endl; logFile.flush(); } };

        struct EquivalenceScenario { std::string Name; DirectX::XMFLOAT3 Pos; DirectX::XMFLOAT3 RotEulerRad; DirectX::XMFLOAT3 Scale; DirectX::XMFLOAT3 MeshPos; DirectX::XMFLOAT3 MeshRotEulerRad; DirectX::XMFLOAT3 MeshScale; };
        const std::vector<EquivalenceScenario> scenarios = {
            {"Identity", {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"PosOffset", {120.f,-45.f,30.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"Yaw90", {0.f,0.f,0.f}, {0.f,1.5707963f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"Pitch1_6", {0.f,0.f,0.f}, {1.60f,0.00f,0.00f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"PosRot", {-80.f,15.f,60.f}, {0.35f,-1.00f,0.65f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"MeshOffsetRot", {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {10.f,-20.f,5.f}, {0.2f,1.2f,0.3f}, {1.f,1.f,1.f}},
        };

        constexpr float kPosTol = 0.015f;
        constexpr float kRotTolDeg = 0.2f;
        constexpr float kRelRotTolDeg = 0.2f;
        constexpr float kDt = 1.0f / 60.0f;
        const int totalFrames = static_cast<int>(std::round(20.0f / kDt));
        Log("[Validate] RootMotion ON/OFF equivalence validation started.");

        AssetRegistry reg;
        reg.SetContentRoot(kContentRoot);
        reg.SetDatabasePath(kRegistryPath);
        if (!reg.LoadFromDisk()) return 21;
        const AssetMeta* meshMeta = FindAssetByVirtualPathOrText(reg, AssetType::SkeletalMesh, kMeshVirtualPath, kMeshSearchText);
        const AssetMeta* animMeta = FindAssetByVirtualPathOrText(reg, AssetType::Animation, kAnimVirtualPath, kAnimSearchText);
        if (!meshMeta || !animMeta) return 22;
        ImportOptions opt{}; opt.bBuildMaterials = false; ImportResult importResult{};
        if (!FbxImporter::ImportFBX(meshMeta->sourcePath, opt, importResult) || importResult.Type != EImportedMeshType::Skeletal || !importResult.SkeletalMesh) return 23;

        bool bOverallPass = true;
        for (const EquivalenceScenario& scn : scenarios)
        {
            Character actorOn; Character actorOff; actorOn.SetName("RM_On"); actorOff.SetName("RM_Off");
            if (!actorOn.GetRootComponent() || !actorOff.GetRootComponent()) return 24;
            actorOn.GetRootComponent()->GetRelativeTransform().SetPosition(scn.Pos.x, scn.Pos.y, scn.Pos.z);
            actorOn.GetRootComponent()->GetRelativeTransform().SetRotationEuler(scn.RotEulerRad.x, scn.RotEulerRad.y, scn.RotEulerRad.z);
            actorOn.GetRootComponent()->GetRelativeTransform().SetScale(scn.Scale.x, scn.Scale.y, scn.Scale.z);
            actorOff.GetRootComponent()->GetRelativeTransform().SetPosition(scn.Pos.x, scn.Pos.y, scn.Pos.z);
            actorOff.GetRootComponent()->GetRelativeTransform().SetRotationEuler(scn.RotEulerRad.x, scn.RotEulerRad.y, scn.RotEulerRad.z);
            actorOff.GetRootComponent()->GetRelativeTransform().SetScale(scn.Scale.x, scn.Scale.y, scn.Scale.z);
            SkeletalMeshComponent* skOn = actorOn.GetSkeletalComponent(); SkeletalMeshComponent* skOff = actorOff.GetSkeletalComponent(); if (!skOn || !skOff) return 25;
            skOn->SetMesh(importResult.SkeletalMesh); skOff->SetMesh(importResult.SkeletalMesh);
            skOn->GetRelativeTransform().SetPosition(scn.MeshPos.x, scn.MeshPos.y, scn.MeshPos.z); skOn->GetRelativeTransform().SetRotationEuler(scn.MeshRotEulerRad.x, scn.MeshRotEulerRad.y, scn.MeshRotEulerRad.z); skOn->GetRelativeTransform().SetScale(scn.MeshScale.x, scn.MeshScale.y, scn.MeshScale.z);
            skOff->GetRelativeTransform().SetPosition(scn.MeshPos.x, scn.MeshPos.y, scn.MeshPos.z); skOff->GetRelativeTransform().SetRotationEuler(scn.MeshRotEulerRad.x, scn.MeshRotEulerRad.y, scn.MeshRotEulerRad.z); skOff->GetRelativeTransform().SetScale(scn.MeshScale.x, scn.MeshScale.y, scn.MeshScale.z);
            Skeleton* skeleton = skOn->GetSkeleton(); if (!skeleton) return 26;
            std::shared_ptr<AnimSequenceAsset> seqBase = FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(animMeta->sourcePath, *skeleton); if (!seqBase || seqBase->Sections.empty()) return 27;
            std::shared_ptr<AnimSequenceAsset> seqOn = std::make_shared<AnimSequenceAsset>(*seqBase); std::shared_ptr<AnimSequenceAsset> seqOff = std::make_shared<AnimSequenceAsset>(*seqBase); for (AnimSection& sec : seqOn->Sections) sec.bEnableRootMotion = true; for (AnimSection& sec : seqOff->Sections) sec.bEnableRootMotion = false;
            AnimInstance* animOn = skOn->GetAnimInstance(); AnimInstance* animOff = skOff->GetAnimInstance(); if (!animOn || !animOff) return 28;
            animOn->SetConsumeRootInPose(true); animOn->SetSequence(seqOn); animOn->Play("Default", true, 1.0f); animOff->SetConsumeRootInPose(false); animOff->SetSequence(seqOff); animOff->Play("Default", true, 1.0f);
            CharacterMovementComponent* moveOn = actorOn.GetMovementComponent(); CharacterMovementComponent* moveOff = actorOff.GetMovementComponent(); if (!moveOn || !moveOff) return 29;
            moveOn->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything); moveOff->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

            const size_t boneCount = skeleton->Bones.size();
            const int headBoneIndex = FindHeadBoneIndex(*skeleton);
            std::vector<DirectX::XMFLOAT4> qOnBase(boneCount, DirectX::XMFLOAT4{0.f,0.f,0.f,1.f}), qOffBase(boneCount, DirectX::XMFLOAT4{0.f,0.f,0.f,1.f});
            std::vector<uint8_t> hasBaseQuat(boneCount, 0);
            float maxPosDiff = 0.0f, maxRotDiffDeg = 0.0f, maxRelRotDiffDeg = 0.0f, headPosMax = 0.0f, headRotMaxDeg = 0.0f, headRelRotMaxDeg = 0.0f;
            int maxPosFrame = 0, maxRotFrame = 0, maxRelRotFrame = 0, headPosFrame = 0, headRotFrame = 0, headRelRotFrame = 0, validMismatchCount = 0, validMismatchFirstFrame = 0, headValidSampleCount = 0;
            size_t maxPosBone = 0, maxRotBone = 0, maxRelRotBone = 0, validMismatchFirstBone = 0;
            std::vector<DirectX::XMFLOAT3> onPos, offPos; std::vector<DirectX::XMFLOAT4> onRot, offRot; std::vector<uint8_t> onValid, offValid;
            for (int frame = 1; frame <= totalFrames; ++frame)
            {
                actorOn.Tick(kDt); actorOff.Tick(kDt);
                ComputeBoneWorldTransforms(actorOn, *skOn, *animOn, *skeleton, onPos, onRot, onValid);
                ComputeBoneWorldTransforms(actorOff, *skOff, *animOff, *skeleton, offPos, offRot, offValid);
                for (size_t bi = 0; bi < boneCount; ++bi)
                {
                    const bool onV = (bi < onValid.size()) && (onValid[bi] != 0); const bool offV = (bi < offValid.size()) && (offValid[bi] != 0);
                    if (onV != offV) { ++validMismatchCount; if (validMismatchFirstFrame == 0) { validMismatchFirstFrame = frame; validMismatchFirstBone = bi; } }
                    if (!onV || !offV || bi >= onRot.size() || bi >= offRot.size() || bi >= onPos.size() || bi >= offPos.size()) continue;
                    const DirectX::XMVECTOR qOn = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&onRot[bi])); const DirectX::XMVECTOR qOff = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&offRot[bi]));
                    const float diffDeg = QuatDiffDeg(qOn, qOff); if (diffDeg > maxRotDiffDeg) { maxRotDiffDeg = diffDeg; maxRotBone = bi; maxRotFrame = frame; }
                    if (!hasBaseQuat[bi]) { DirectX::XMStoreFloat4(&qOnBase[bi], qOn); DirectX::XMStoreFloat4(&qOffBase[bi], qOff); hasBaseQuat[bi] = 1; }
                    const DirectX::XMVECTOR qOnRel = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&qOnBase[bi])), qOn);
                    const DirectX::XMVECTOR qOffRel = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&qOffBase[bi])), qOff);
                    const float diffDegRel = QuatDiffDeg(qOnRel, qOffRel); if (diffDegRel > maxRelRotDiffDeg) { maxRelRotDiffDeg = diffDegRel; maxRelRotBone = bi; maxRelRotFrame = frame; }
                    const float dx = onPos[bi].x - offPos[bi].x, dy = onPos[bi].y - offPos[bi].y, dz = onPos[bi].z - offPos[bi].z; const float posDiff = std::sqrt(dx * dx + dy * dy + dz * dz); if (posDiff > maxPosDiff) { maxPosDiff = posDiff; maxPosBone = bi; maxPosFrame = frame; }
                    if (static_cast<int>(bi) == headBoneIndex) { ++headValidSampleCount; if (posDiff > headPosMax) { headPosMax = posDiff; headPosFrame = frame; } if (diffDeg > headRotMaxDeg) { headRotMaxDeg = diffDeg; headRotFrame = frame; } if (diffDegRel > headRelRotMaxDeg) { headRelRotMaxDeg = diffDegRel; headRelRotFrame = frame; } }
                }
            }

            const bool bScenarioPass = (maxPosDiff <= kPosTol) && (maxRotDiffDeg <= kRotTolDeg) && (maxRelRotDiffDeg <= kRelRotTolDeg) && (validMismatchCount == 0) && (headBoneIndex < 0 || headValidSampleCount > 0);
            if (!bScenarioPass) bOverallPass = false;
            std::ostringstream line;
            line << std::fixed << std::setprecision(6) << "[Validate][FullDelta][" << scn.Name << "] " << (bScenarioPass ? "PASS" : "FAIL") << " PosMax=" << maxPosDiff << " RotMaxDeg=" << maxRotDiffDeg << " RelRotMaxDeg=" << maxRelRotDiffDeg << " HeadPosMax=" << headPosMax << " HeadRotMaxDeg=" << headRotMaxDeg << " HeadRelRotMaxDeg=" << headRelRotMaxDeg << " ValidMismatchCount=" << validMismatchCount;
            Log(line.str());
        }

        Log(std::string("[Validate][Summary] FinalResult=") + (bOverallPass ? "PASS" : "FAIL"));
        return bOverallPass ? 0 : 30;
    }

    int RunRootMotionRuntimeValidationCli(int frameCountOverride)
    {
        const std::string kContentRoot = "D:\\Projects\\JMEngine\\Contents\\";
        const std::string kRegistryPath = "D:\\Projects\\JMEngine\\AssetRegistry.json";
        const std::string kMeshVirtualPath = "/Game/Mesh/passive_marker_man";
        const std::string kAnimVirtualPath = "/Game/Animation/Capoeira";
        const std::string kMeshSearchText = "passive_marker_man";
        const std::string kAnimSearchText = "capoeira";
        const std::string kLogPath = "D:\\Projects\\JMEngine\\JMEngine\\x64\\Debug\\RootMotionRuntimeValidationCLI.log";
        const std::string kCsvPath = "D:\\Projects\\JMEngine\\JMEngine\\x64\\Debug\\RootMotionRuntimeValidationCLI_FrameBone.csv";
        std::ofstream logFile(kLogPath, std::ios::out | std::ios::trunc);
        std::ofstream csvFile(kCsvPath, std::ios::out | std::ios::trunc);
        auto Log = [&](const std::string& text) { std::cout << text << std::endl; if (logFile.is_open()) { logFile << text << std::endl; logFile.flush(); } };
        if (csvFile.is_open()) { csvFile << "scenario,frame,bone_index,bone_name,pos_diff,rot_diff_deg,rel_rot_diff_deg\n"; csvFile.flush(); }

        struct RuntimeScenario { std::string Name; DirectX::XMFLOAT3 Pos; DirectX::XMFLOAT3 RotEulerRad; DirectX::XMFLOAT3 Scale; DirectX::XMFLOAT3 MeshPos; DirectX::XMFLOAT3 MeshRotEulerRad; DirectX::XMFLOAT3 MeshScale; };
        const std::vector<RuntimeScenario> scenarios = {
            {"Identity", {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"Pitch1_6", {0.f,0.f,0.f}, {1.6f,0.f,0.f}, {1.f,1.f,1.f}, {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}},
            {"MeshOffsetRot", {0.f,0.f,0.f}, {0.f,0.f,0.f}, {1.f,1.f,1.f}, {10.f,-20.f,5.f}, {0.2f,1.2f,0.3f}, {1.f,1.f,1.f}},
        };

        constexpr float kPosTol = 0.015f, kRotTolDeg = 0.2f, kRelRotTolDeg = 0.2f, kRootDriftPosTol = 0.01f, kRootDriftRotTolDeg = 0.5f, kDt = 1.0f / 60.0f;
        const int kFrames = (std::max)(60, frameCountOverride > 0 ? frameCountOverride : 1200);
        Log("[RuntimeValidateCLI] Started.");

        AssetRegistry reg; reg.SetContentRoot(kContentRoot); reg.SetDatabasePath(kRegistryPath); if (!reg.LoadFromDisk()) return 41;
        const AssetMeta* meshMeta = FindAssetByVirtualPathOrText(reg, AssetType::SkeletalMesh, kMeshVirtualPath, kMeshSearchText); const AssetMeta* animMeta = FindAssetByVirtualPathOrText(reg, AssetType::Animation, kAnimVirtualPath, kAnimSearchText); if (!meshMeta || !animMeta) return 42;
        ImportOptions opt{}; opt.bBuildMaterials = false; ImportResult importResult{}; if (!FbxImporter::ImportFBX(meshMeta->sourcePath, opt, importResult) || importResult.Type != EImportedMeshType::Skeletal || !importResult.SkeletalMesh) return 43;

        bool bOverallPass = true;
        for (const RuntimeScenario& scn : scenarios)
        {
            World world; Character* actorOn = world.SpawnActor<Character>("RM_On"); Character* actorOff = world.SpawnActor<Character>("RM_Off"); if (!actorOn || !actorOff) return 44; if (!actorOn->GetRootComponent() || !actorOff->GetRootComponent()) return 45;
            actorOn->GetRootComponent()->GetRelativeTransform().SetPosition(scn.Pos.x, scn.Pos.y, scn.Pos.z); actorOn->GetRootComponent()->GetRelativeTransform().SetRotationEuler(scn.RotEulerRad.x, scn.RotEulerRad.y, scn.RotEulerRad.z); actorOn->GetRootComponent()->GetRelativeTransform().SetScale(scn.Scale.x, scn.Scale.y, scn.Scale.z);
            actorOff->GetRootComponent()->GetRelativeTransform().SetPosition(scn.Pos.x, scn.Pos.y, scn.Pos.z); actorOff->GetRootComponent()->GetRelativeTransform().SetRotationEuler(scn.RotEulerRad.x, scn.RotEulerRad.y, scn.RotEulerRad.z); actorOff->GetRootComponent()->GetRelativeTransform().SetScale(scn.Scale.x, scn.Scale.y, scn.Scale.z);
            SkeletalMeshComponent* skOn = actorOn->GetSkeletalComponent(); SkeletalMeshComponent* skOff = actorOff->GetSkeletalComponent(); if (!skOn || !skOff) return 46;
            skOn->SetMesh(importResult.SkeletalMesh); skOff->SetMesh(importResult.SkeletalMesh);
            skOn->GetRelativeTransform().SetPosition(scn.MeshPos.x, scn.MeshPos.y, scn.MeshPos.z); skOn->GetRelativeTransform().SetRotationEuler(scn.MeshRotEulerRad.x, scn.MeshRotEulerRad.y, scn.MeshRotEulerRad.z); skOn->GetRelativeTransform().SetScale(scn.MeshScale.x, scn.MeshScale.y, scn.MeshScale.z);
            skOff->GetRelativeTransform().SetPosition(scn.MeshPos.x, scn.MeshPos.y, scn.MeshPos.z); skOff->GetRelativeTransform().SetRotationEuler(scn.MeshRotEulerRad.x, scn.MeshRotEulerRad.y, scn.MeshRotEulerRad.z); skOff->GetRelativeTransform().SetScale(scn.MeshScale.x, scn.MeshScale.y, scn.MeshScale.z);
            Skeleton* skeleton = skOn->GetSkeleton(); if (!skeleton) return 47; std::shared_ptr<AnimSequenceAsset> seqBase = FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(animMeta->sourcePath, *skeleton); if (!seqBase || seqBase->Sections.empty()) return 48;
            std::shared_ptr<AnimSequenceAsset> seqOn = std::make_shared<AnimSequenceAsset>(*seqBase), seqOff = std::make_shared<AnimSequenceAsset>(*seqBase); for (AnimSection& sec : seqOn->Sections) sec.bEnableRootMotion = true; for (AnimSection& sec : seqOff->Sections) sec.bEnableRootMotion = false;
            AnimInstance* animOn = skOn->GetAnimInstance(); AnimInstance* animOff = skOff->GetAnimInstance(); if (!animOn || !animOff) return 49;
            animOn->SetConsumeRootInPose(true); animOn->SetSequence(seqOn); animOn->Play("Default", true, 1.0f); animOff->SetConsumeRootInPose(false); animOff->SetSequence(seqOff); animOff->Play("Default", true, 1.0f);
            CharacterMovementComponent* moveOn = actorOn->GetMovementComponent(); CharacterMovementComponent* moveOff = actorOff->GetMovementComponent(); if (!moveOn || !moveOff) return 50; moveOn->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything); moveOff->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

            std::vector<DirectX::XMFLOAT3> onPos, offPos; std::vector<DirectX::XMFLOAT4> onRot, offRot; std::vector<uint8_t> onValid, offValid;
            const size_t boneCount = skeleton->Bones.size(); const int headBoneIndex = FindHeadBoneIndex(*skeleton); const int rootBoneIndex = FindRootBoneIndex(*skeleton);
            std::vector<DirectX::XMFLOAT4> qOnBase(boneCount, DirectX::XMFLOAT4{0.f,0.f,0.f,1.f}), qOffBase(boneCount, DirectX::XMFLOAT4{0.f,0.f,0.f,1.f}); std::vector<uint8_t> hasBaseQuat(boneCount, 0);
            float maxPosDiff = 0.0f, maxRotDiffDeg = 0.0f, maxRelRotDiffDeg = 0.0f, headPosMax = 0.0f, headRotMaxDeg = 0.0f, headRelRotMaxDeg = 0.0f, rootDriftOnPosMax = 0.0f, rootDriftOnRotMaxDeg = 0.0f, rootDriftOffPosMax = 0.0f, rootDriftOffRotMaxDeg = 0.0f;
            int maxPosFrame = 0, maxRotFrame = 0, maxRelRotFrame = 0, headPosFrame = 0, headRotFrame = 0, headRelRotFrame = 0, validMismatchCount = 0, validMismatchFirstFrame = 0, headValidSampleCount = 0, rootDriftOnPosFrame = 0, rootDriftOnRotFrame = 0, rootDriftOffPosFrame = 0, rootDriftOffRotFrame = 0;
            size_t maxPosBone = 0, maxRotBone = 0, maxRelRotBone = 0, validMismatchFirstBone = 0;
            bool hasRootBaseOn = false, hasRootBaseOff = false; DirectX::XMFLOAT3 rootBaseOnPosActor{}, rootBaseOffPosActor{}; DirectX::XMFLOAT4 rootBaseOnRotActor{0.f,0.f,0.f,1.f}, rootBaseOffRotActor{0.f,0.f,0.f,1.f};
            for (int frame = 1; frame <= kFrames; ++frame)
            {
                world.Tick(kDt); ComputeBoneWorldTransforms(*actorOn, *skOn, *animOn, *skeleton, onPos, onRot, onValid); ComputeBoneWorldTransforms(*actorOff, *skOff, *animOff, *skeleton, offPos, offRot, offValid);
                if (rootBoneIndex >= 0)
                {
                    const size_t rbi = static_cast<size_t>(rootBoneIndex); const bool onRootValid = (rbi < onValid.size()) && (onValid[rbi] != 0) && (rbi < onPos.size()) && (rbi < onRot.size()); const bool offRootValid = (rbi < offValid.size()) && (offValid[rbi] != 0) && (rbi < offPos.size()) && (rbi < offRot.size());
                    if (onRootValid && offRootValid)
                    {
                        const Transform& actorRootOn = actorOn->GetRootComponent()->GetRelativeTransform(); const Transform& actorRootOff = actorOff->GetRootComponent()->GetRelativeTransform(); const DirectX::XMVECTOR qActorOn = DirectX::XMQuaternionNormalize(actorRootOn.GetRotationQuat()); const DirectX::XMVECTOR qActorOff = DirectX::XMQuaternionNormalize(actorRootOff.GetRotationQuat());
                        const DirectX::XMVECTOR qBoneOn = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&onRot[rbi])); const DirectX::XMVECTOR qBoneOff = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&offRot[rbi]));
                        const DirectX::XMVECTOR vOnDiffWorld = DirectX::XMVectorSet(onPos[rbi].x - actorRootOn.m_Pos.x, onPos[rbi].y - actorRootOn.m_Pos.y, onPos[rbi].z - actorRootOn.m_Pos.z, 0.f); const DirectX::XMVECTOR vOffDiffWorld = DirectX::XMVectorSet(offPos[rbi].x - actorRootOff.m_Pos.x, offPos[rbi].y - actorRootOff.m_Pos.y, offPos[rbi].z - actorRootOff.m_Pos.z, 0.f);
                        const DirectX::XMVECTOR vOnActorSpace = DirectX::XMVector3Rotate(vOnDiffWorld, DirectX::XMQuaternionInverse(qActorOn)); const DirectX::XMVECTOR vOffActorSpace = DirectX::XMVector3Rotate(vOffDiffWorld, DirectX::XMQuaternionInverse(qActorOff));
                        const DirectX::XMVECTOR qOnRootInActor = DirectX::XMQuaternionMultiply(qBoneOn, DirectX::XMQuaternionInverse(qActorOn)); const DirectX::XMVECTOR qOffRootInActor = DirectX::XMQuaternionMultiply(qBoneOff, DirectX::XMQuaternionInverse(qActorOff));
                        DirectX::XMFLOAT3 onPosActor{}, offPosActor{}; DirectX::XMFLOAT4 onRotActor{}, offRotActor{}; DirectX::XMStoreFloat3(&onPosActor, vOnActorSpace); DirectX::XMStoreFloat3(&offPosActor, vOffActorSpace); DirectX::XMStoreFloat4(&onRotActor, qOnRootInActor); DirectX::XMStoreFloat4(&offRotActor, qOffRootInActor);
                        if (!hasRootBaseOn) { rootBaseOnPosActor = onPosActor; rootBaseOnRotActor = onRotActor; hasRootBaseOn = true; } if (!hasRootBaseOff) { rootBaseOffPosActor = offPosActor; rootBaseOffRotActor = offRotActor; hasRootBaseOff = true; }
                        const float onRootDriftPos = std::sqrt((onPosActor.x - rootBaseOnPosActor.x)*(onPosActor.x - rootBaseOnPosActor.x) + (onPosActor.y - rootBaseOnPosActor.y)*(onPosActor.y - rootBaseOnPosActor.y) + (onPosActor.z - rootBaseOnPosActor.z)*(onPosActor.z - rootBaseOnPosActor.z)); const float onRootDriftRotDeg = QuatDiffDeg(DirectX::XMLoadFloat4(&onRotActor), DirectX::XMLoadFloat4(&rootBaseOnRotActor)); if (onRootDriftPos > rootDriftOnPosMax) { rootDriftOnPosMax = onRootDriftPos; rootDriftOnPosFrame = frame; } if (onRootDriftRotDeg > rootDriftOnRotMaxDeg) { rootDriftOnRotMaxDeg = onRootDriftRotDeg; rootDriftOnRotFrame = frame; }
                        const float offRootDriftPos = std::sqrt((offPosActor.x - rootBaseOffPosActor.x)*(offPosActor.x - rootBaseOffPosActor.x) + (offPosActor.y - rootBaseOffPosActor.y)*(offPosActor.y - rootBaseOffPosActor.y) + (offPosActor.z - rootBaseOffPosActor.z)*(offPosActor.z - rootBaseOffPosActor.z)); const float offRootDriftRotDeg = QuatDiffDeg(DirectX::XMLoadFloat4(&offRotActor), DirectX::XMLoadFloat4(&rootBaseOffRotActor)); if (offRootDriftPos > rootDriftOffPosMax) { rootDriftOffPosMax = offRootDriftPos; rootDriftOffPosFrame = frame; } if (offRootDriftRotDeg > rootDriftOffRotMaxDeg) { rootDriftOffRotMaxDeg = offRootDriftRotDeg; rootDriftOffRotFrame = frame; }
                    }
                }
                for (size_t bi = 0; bi < boneCount; ++bi)
                {
                    const bool onV = (bi < onValid.size()) && (onValid[bi] != 0), offV = (bi < offValid.size()) && (offValid[bi] != 0); if (onV != offV) { ++validMismatchCount; if (validMismatchFirstFrame == 0) { validMismatchFirstFrame = frame; validMismatchFirstBone = bi; } } if (!onV || !offV || bi >= onRot.size() || bi >= offRot.size() || bi >= onPos.size() || bi >= offPos.size()) continue;
                    const DirectX::XMVECTOR qOn = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&onRot[bi])), qOff = DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&offRot[bi])); const float rotDiffDeg = QuatDiffDeg(qOn, qOff); if (rotDiffDeg > maxRotDiffDeg) { maxRotDiffDeg = rotDiffDeg; maxRotBone = bi; maxRotFrame = frame; }
                    if (!hasBaseQuat[bi]) { DirectX::XMStoreFloat4(&qOnBase[bi], qOn); DirectX::XMStoreFloat4(&qOffBase[bi], qOff); hasBaseQuat[bi] = 1; }
                    const DirectX::XMVECTOR qOnRel = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&qOnBase[bi])), qOn); const DirectX::XMVECTOR qOffRel = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&qOffBase[bi])), qOff); const float relRotDiffDeg = QuatDiffDeg(qOnRel, qOffRel); if (relRotDiffDeg > maxRelRotDiffDeg) { maxRelRotDiffDeg = relRotDiffDeg; maxRelRotBone = bi; maxRelRotFrame = frame; }
                    const float dx = onPos[bi].x - offPos[bi].x, dy = onPos[bi].y - offPos[bi].y, dz = onPos[bi].z - offPos[bi].z; const float posDiff = std::sqrt(dx * dx + dy * dy + dz * dz); if (posDiff > maxPosDiff) { maxPosDiff = posDiff; maxPosBone = bi; maxPosFrame = frame; }
                    if (static_cast<int>(bi) == headBoneIndex) { ++headValidSampleCount; if (posDiff > headPosMax) { headPosMax = posDiff; headPosFrame = frame; } if (rotDiffDeg > headRotMaxDeg) { headRotMaxDeg = rotDiffDeg; headRotFrame = frame; } if (relRotDiffDeg > headRelRotMaxDeg) { headRelRotMaxDeg = relRotDiffDeg; headRelRotFrame = frame; } }
                    if (csvFile.is_open()) { const std::string boneName = (bi < skeleton->Bones.size()) ? skeleton->Bones[bi].Name : std::string("N/A"); csvFile << "\"" << scn.Name << "\"," << frame << "," << bi << ",\"" << boneName << "\"," << std::fixed << std::setprecision(6) << posDiff << "," << rotDiffDeg << "," << relRotDiffDeg << "\n"; }
                }
            }
            const bool bScenarioPass = (maxPosDiff <= kPosTol) && (maxRotDiffDeg <= kRotTolDeg) && (maxRelRotDiffDeg <= kRelRotTolDeg) && (validMismatchCount == 0) && (headBoneIndex < 0 || headValidSampleCount > 0) && (rootBoneIndex < 0 || (hasRootBaseOn && rootDriftOnPosMax <= kRootDriftPosTol && rootDriftOnRotMaxDeg <= kRootDriftRotTolDeg));
            if (!bScenarioPass) bOverallPass = false;
            std::ostringstream line; line << std::fixed << std::setprecision(6) << "[RuntimeValidateCLI][" << scn.Name << "] " << (bScenarioPass ? "PASS" : "FAIL") << " PosMax=" << maxPosDiff << " RotMaxDeg=" << maxRotDiffDeg << " RelRotMaxDeg=" << maxRelRotDiffDeg << " HeadPosMax=" << headPosMax << " HeadRotMaxDeg=" << headRotMaxDeg << " HeadRelRotMaxDeg=" << headRelRotMaxDeg << " RootDriftOn(Pos=" << rootDriftOnPosMax << "@F" << rootDriftOnPosFrame << ",RotDeg=" << rootDriftOnRotMaxDeg << "@F" << rootDriftOnRotFrame << ") RootDriftOff(Pos=" << rootDriftOffPosMax << "@F" << rootDriftOffPosFrame << ",RotDeg=" << rootDriftOffRotMaxDeg << "@F" << rootDriftOffRotFrame << ") ValidMismatchCount=" << validMismatchCount; Log(line.str());
        }
        Log(std::string("[RuntimeValidateCLI][Summary] FinalResult=") + (bOverallPass ? "PASS" : "FAIL"));
        return bOverallPass ? 0 : 51;
    }
}

namespace Test
{
    bool TryRunRootMotionHeadlessCli(int argc, char** argv, int& outExitCode)
    {
        outExitCode = 0;
        if (argc < 2)
        {
            return false;
        }

        const std::string mode = argv[1];
        if (mode == "--rootmotion-probe-headless")
        {
            outExitCode = RunRootMotionProbeHeadless();
            return true;
        }
        if (mode == "--rootmotion-validate-equivalence")
        {
            outExitCode = RunRootMotionEquivalenceValidationHeadless();
            return true;
        }
        if (mode == "--rootmotion-runtime-validate-cli")
        {
            int frames = 1200;
            if (argc >= 3)
            {
                frames = std::atoi(argv[2]);
            }
            outExitCode = RunRootMotionRuntimeValidationCli(frames);
            return true;
        }
        return false;
    }
}
