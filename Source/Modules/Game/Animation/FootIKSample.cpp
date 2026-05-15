#include "FootIKSample.h"

#include "../Skeletal/SkeletalMeshComponent.h"
#include "../Skeletal/SkeletalMeshData.h"
#include "../World.h"
#include "../Collision/WorldQuery.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string.h>

#include <nlohmann/json.hpp>

using namespace DirectX;

namespace
{
    using json = nlohmann::json;

    FootIKSampleTuning g_Tuning{};

    struct FootIKLegProfileData
    {
        std::string Thigh;
        std::string Calf;
        std::string Foot;
        std::string Toe;
        std::string Heel;
        std::string KneeHint;
    };

    struct FootIKBoneChainProfileData
    {
        std::string Key;
        std::string MeshVirtualPath;
        std::string MeshSourcePath;
        std::string Pelvis;
        FootIKLegProfileData Left;
        FootIKLegProfileData Right;
    };

    struct FootIKProfileDatabase
    {
        bool bLoaded = false;
        std::vector<FootIKBoneChainProfileData> Profiles;
    };

    static bool ContainsInsensitive(const std::string& text, const std::string& token)
    {
        if (token.empty() || text.empty()) return false;
        std::string a = text;
        std::string b = token;
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return a.find(b) != std::string::npos;
    }

    static int32_t FindLegParentIndex(const SkeletalMeshAsset* mesh, int32_t fromBone, const char* tokenA, const char* tokenB)
    {
        if (!mesh || fromBone < 0 || fromBone >= (int32_t)mesh->Skeleton.Bones.size()) return -1;
        int32_t cur = mesh->Skeleton.Bones[fromBone].ParentIndex;
        while (cur >= 0 && cur < (int32_t)mesh->Skeleton.Bones.size())
        {
            const std::string& n = mesh->Skeleton.Bones[cur].Name;
            if (ContainsInsensitive(n, tokenA) || ContainsInsensitive(n, tokenB)) return cur;
            cur = mesh->Skeleton.Bones[cur].ParentIndex;
        }
        return -1;
    }

    static int32_t FindBoneIndexByNameExactInsensitive(const SkeletalMeshAsset* mesh, const std::string& wanted)
    {
        if (!mesh || wanted.empty()) return -1;
        const auto& bones = mesh->Skeleton.Bones;
        for (size_t i = 0; i < bones.size(); ++i)
        {
            const std::string& name = bones[i].Name;
            if (name.size() != wanted.size()) continue;
            bool bEqual = true;
            for (size_t c = 0; c < name.size(); ++c)
            {
                if ((char)std::tolower((unsigned char)name[c]) != (char)std::tolower((unsigned char)wanted[c]))
                {
                    bEqual = false;
                    break;
                }
            }
            if (bEqual) return (int32_t)i;
        }
        return -1;
    }

    static int32_t FindBoneIndexByTokenInsensitive(const SkeletalMeshAsset* mesh, const char* tokenA, const char* tokenB = nullptr)
    {
        if (!mesh) return -1;
        const auto& bones = mesh->Skeleton.Bones;
        for (size_t i = 0; i < bones.size(); ++i)
        {
            const std::string& name = bones[i].Name;
            if (tokenA && ContainsInsensitive(name, tokenA)) return (int32_t)i;
            if (tokenB && ContainsInsensitive(name, tokenB)) return (int32_t)i;
        }
        return -1;
    }

    static int32_t FindFootBoneIndexFromMesh(const SkeletalMeshAsset* mesh, bool bLeftFoot)
    {
        if (!mesh) return -1;

        const char* left[] = { "foot_l", "l_foot", "leftfoot", "left_foot" };
        const char* right[] = { "foot_r", "r_foot", "rightfoot", "right_foot" };

        const auto& bones = mesh->Skeleton.Bones;
        for (size_t i = 0; i < bones.size(); ++i)
        {
            const std::string& name = bones[i].Name;
            const char** candidates = bLeftFoot ? left : right;
            const size_t count = bLeftFoot ? std::size(left) : std::size(right);
            for (size_t c = 0; c < count; ++c)
            {
                if (ContainsInsensitive(name, candidates[c])) return (int32_t)i;
            }
        }

        return -1;
    }

    static std::string GetJsonString(const json& node, const char* key)
    {
        if (!node.is_object() || !node.contains(key) || !node[key].is_string()) return {};
        return node[key].get<std::string>();
    }

    static FootIKProfileDatabase& GetFootIKProfileDatabase()
    {
        static FootIKProfileDatabase db;
        if (db.bLoaded) return db;

        db.bLoaded = true;

        const std::filesystem::path candidates[] = {
            std::filesystem::path("Contents/Config/FootIKProfiles.json"),
            std::filesystem::path("Contents/FootIKProfiles.json")
        };

        std::filesystem::path selectedPath;
        for (const std::filesystem::path& p : candidates)
        {
            if (std::filesystem::exists(p))
            {
                selectedPath = p;
                break;
            }
        }

        if (selectedPath.empty()) return db;

        std::ifstream in(selectedPath);
        if (!in.is_open()) return db;

        json root;
        try
        {
            in >> root;
        }
        catch (...)
        {
            return db;
        }

        if (!root.is_object() || !root.contains("profiles") || !root["profiles"].is_array()) return db;

        for (const json& p : root["profiles"])
        {
            if (!p.is_object()) continue;

            FootIKBoneChainProfileData profile;
            profile.Key = GetJsonString(p, "key");
            profile.MeshVirtualPath = GetJsonString(p, "mesh");
            profile.MeshSourcePath = GetJsonString(p, "source");

            const json bones = p.contains("bones") && p["bones"].is_object() ? p["bones"] : json::object();
            profile.Pelvis = GetJsonString(bones, "pelvis");

            const json left = bones.contains("left") && bones["left"].is_object() ? bones["left"] : json::object();
            profile.Left.Thigh = GetJsonString(left, "thigh");
            profile.Left.Calf = GetJsonString(left, "calf");
            profile.Left.Foot = GetJsonString(left, "foot");
            profile.Left.Toe = GetJsonString(left, "toe");
            profile.Left.Heel = GetJsonString(left, "heel");
            profile.Left.KneeHint = GetJsonString(left, "kneeHint");

            const json right = bones.contains("right") && bones["right"].is_object() ? bones["right"] : json::object();
            profile.Right.Thigh = GetJsonString(right, "thigh");
            profile.Right.Calf = GetJsonString(right, "calf");
            profile.Right.Foot = GetJsonString(right, "foot");
            profile.Right.Toe = GetJsonString(right, "toe");
            profile.Right.Heel = GetJsonString(right, "heel");
            profile.Right.KneeHint = GetJsonString(right, "kneeHint");

            db.Profiles.push_back(std::move(profile));
        }

        return db;
    }

    static const FootIKBoneChainProfileData* FindProfileForMesh(const SkeletalMeshAsset* mesh)
    {
        if (!mesh) return nullptr;

        const FootIKProfileDatabase& db = GetFootIKProfileDatabase();
        if (db.Profiles.empty()) return nullptr;

        const std::string& virtualPath = mesh->GetAssetVirtualPath();
        const std::string& sourcePath = mesh->GetAssetSourcePath();

        for (const FootIKBoneChainProfileData& p : db.Profiles)
        {
            const bool bVirtualMatch = !p.MeshVirtualPath.empty() && !virtualPath.empty() && _stricmp(p.MeshVirtualPath.c_str(), virtualPath.c_str()) == 0;
            const bool bSourceMatch = !p.MeshSourcePath.empty() && !sourcePath.empty() && _stricmp(p.MeshSourcePath.c_str(), sourcePath.c_str()) == 0;
            if (bVirtualMatch || bSourceMatch)
            {
                return &p;
            }
        }

        return nullptr;
    }

    static XMFLOAT3 GetTranslation(const XMFLOAT4X4& m)
    {
        return XMFLOAT3(m._41, m._42, m._43);
    }

    static XMFLOAT3 GetBoneWorldLocation(const XMFLOAT4X4& boneGlobal, const XMMATRIX& componentWorld)
    {
        const XMMATRIX boneM = XMLoadFloat4x4(&boneGlobal);
        const XMMATRIX boneWorld = boneM * componentWorld;
        XMFLOAT3 out{};
        XMStoreFloat3(&out, boneWorld.r[3]);
        return out;
    }

    static XMFLOAT3 GetWorldAxisNormalized(const XMFLOAT4X4& boneGlobal, const XMMATRIX& componentWorld, int axisIndex)
    {
        const XMMATRIX boneM = XMLoadFloat4x4(&boneGlobal);
        const XMMATRIX boneWorld = boneM * componentWorld;
        XMVECTOR axis = boneWorld.r[axisIndex];
        axis = XMVectorSetW(axis, 0.0f);
        if (XMVectorGetX(XMVector3LengthSq(axis)) <= 1e-6f) return XMFLOAT3(0, 0, 1);
        XMFLOAT3 out{};
        XMStoreFloat3(&out, XMVector3Normalize(axis));
        return out;
    }

    static XMFLOAT3 Normalize3(const XMFLOAT3& v)
    {
        XMVECTOR x = XMLoadFloat3(&v);
        if (XMVectorGetX(XMVector3LengthSq(x)) <= 1e-8f) return XMFLOAT3(0, 1, 0);
        XMFLOAT3 out{};
        XMStoreFloat3(&out, XMVector3Normalize(x));
        return out;
    }

    static XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return XMFLOAT3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
    }

    static void AddLocalYOffset(XMFLOAT4X4& local, float offsetY)
    {
        local._42 += offsetY;
    }

    static float MoveToward(float current, float target, float maxDelta)
    {
        const float d = target - current;
        if (std::fabs(d) <= maxDelta) return target;
        return current + (d > 0.0f ? maxDelta : -maxDelta);
    }

    static void AlignFootToNormalLocalApprox(XMFLOAT4X4& localFoot, const XMFLOAT3& normal, float strength, float maxTiltDegrees)
    {
        XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&normal));
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        float ndot = XMVectorGetX(XMVector3Dot(n, up));
        ndot = std::clamp(ndot, -1.0f, 1.0f);
        const float fullAngle = std::acos(ndot);
        const float limit = XMConvertToRadians((std::max)(0.0f, maxTiltDegrees));
        const float angle = (std::min)(fullAngle, limit) * std::clamp(strength, 0.0f, 1.0f);

        XMVECTOR axis = XMVector3Cross(up, n);
        if (XMVectorGetX(XMVector3LengthSq(axis)) <= 1e-6f || angle <= 1e-5f) return;

        axis = XMVector3Normalize(axis);
        XMMATRIX rot = XMMatrixRotationAxis(axis, angle);

        XMMATRIX m = XMLoadFloat4x4(&localFoot);
        XMVECTOR t = m.r[3];
        XMMATRIX out = rot * m;
        out.r[3] = t;
        XMStoreFloat4x4(&localFoot, out);
    }

    static XMVECTOR SafeNormalizeVec(FXMVECTOR v, FXMVECTOR fallback)
    {
        if (XMVectorGetX(XMVector3LengthSq(v)) <= 1e-8f)
        {
            return fallback;
        }
        return XMVector3Normalize(v);
    }

    static XMVECTOR QuaternionFromTo(FXMVECTOR fromDir, FXMVECTOR toDir)
    {
        XMVECTOR a = SafeNormalizeVec(fromDir, XMVectorSet(1, 0, 0, 0));
        XMVECTOR b = SafeNormalizeVec(toDir, XMVectorSet(1, 0, 0, 0));
        float dotAB = std::clamp(XMVectorGetX(XMVector3Dot(a, b)), -1.0f, 1.0f);
        if (dotAB > 0.9999f)
        {
            return XMQuaternionIdentity();
        }
        if (dotAB < -0.9999f)
        {
            XMVECTOR axis = XMVector3Cross(a, XMVectorSet(0, 1, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(axis)) <= 1e-8f)
            {
                axis = XMVector3Cross(a, XMVectorSet(0, 0, 1, 0));
            }
            axis = SafeNormalizeVec(axis, XMVectorSet(1, 0, 0, 0));
            return XMQuaternionRotationAxis(axis, XM_PI);
        }

        XMVECTOR axis = SafeNormalizeVec(XMVector3Cross(a, b), XMVectorSet(1, 0, 0, 0));
        float angle = std::acos(dotAB);
        return XMQuaternionRotationAxis(axis, angle);
    }

    static void ApplyQuaternionRotation(XMFLOAT4X4& local, FXMVECTOR rotationQ, float blendAlpha)
    {
        blendAlpha = std::clamp(blendAlpha, 0.0f, 1.0f);
        if (blendAlpha <= 1e-5f) return;

        XMVECTOR q = XMQuaternionSlerp(XMQuaternionIdentity(), XMQuaternionNormalize(rotationQ), blendAlpha);
        XMMATRIX rot = XMMatrixRotationQuaternion(q);

        XMMATRIX m = XMLoadFloat4x4(&local);
        XMVECTOR t = m.r[3];
        m = rot * m;
        m.r[3] = t;
        XMStoreFloat4x4(&local, m);
    }

    static bool TryApplyTwoBoneIKVertical(
        std::vector<XMFLOAT4X4>& inOutLocalPose,
        int32_t upperIdx,
        int32_t lowerIdx,
        int32_t footIdx,
        int32_t kneeHintIdx,
        int32_t toeIdx,
        int32_t heelIdx,
        float effectorOffsetY,
        float blendAlpha,
        float maxStretchRatio,
        bool& bOutKneeHintUsed,
        bool& bOutToeHeelUsed,
        float& outReachRatio,
        std::string& outQuality,
        std::string& outFallbackReason)
    {
        bOutKneeHintUsed = false;
        bOutToeHeelUsed = false;
        outReachRatio = 0.0f;
        outQuality = "invalid";
        outFallbackReason.clear();

        if (upperIdx < 0 || lowerIdx < 0 || footIdx < 0)
        {
            outFallbackReason = "invalid chain";
            return false;
        }
        if (upperIdx >= (int32_t)inOutLocalPose.size() || lowerIdx >= (int32_t)inOutLocalPose.size() || footIdx >= (int32_t)inOutLocalPose.size())
        {
            outFallbackReason = "index oob";
            return false;
        }

        const XMFLOAT3 upperToKneeF = GetTranslation(inOutLocalPose[(size_t)lowerIdx]);
        const XMFLOAT3 kneeToFootF = GetTranslation(inOutLocalPose[(size_t)footIdx]);
        const XMVECTOR upperToKnee = XMLoadFloat3(&upperToKneeF);
        const XMVECTOR kneeToFoot = XMLoadFloat3(&kneeToFootF);

        const float thighLen = std::sqrt(XMVectorGetX(XMVector3LengthSq(upperToKnee)));
        const float calfLen = std::sqrt(XMVectorGetX(XMVector3LengthSq(kneeToFoot)));
        if (thighLen <= 1e-3f || calfLen <= 1e-3f)
        {
            outFallbackReason = "zero limb length";
            return false;
        }

        XMVECTOR footRest = upperToKnee + kneeToFoot;
        XMVECTOR target = footRest + XMVectorSet(0.0f, effectorOffsetY, 0.0f, 0.0f);

        if (toeIdx >= 0 && heelIdx >= 0 && toeIdx < (int32_t)inOutLocalPose.size() && heelIdx < (int32_t)inOutLocalPose.size())
        {
            const XMFLOAT3 toeLocalF = GetTranslation(inOutLocalPose[(size_t)toeIdx]);
            const XMFLOAT3 heelLocalF = GetTranslation(inOutLocalPose[(size_t)heelIdx]);
            const XMVECTOR toeLocal = XMLoadFloat3(&toeLocalF);
            const XMVECTOR heelLocal = XMLoadFloat3(&heelLocalF);
            const XMVECTOR footDir = SafeNormalizeVec(toeLocal - heelLocal, SafeNormalizeVec(footRest, XMVectorSet(0, 0, 1, 0)));
            target = target + footDir * (0.05f * effectorOffsetY);
            bOutToeHeelUsed = true;
        }

        const float targetDistRaw = std::sqrt(XMVectorGetX(XMVector3LengthSq(target)));
        const float maxReach = (thighLen + calfLen) * (std::max)(1.0f, maxStretchRatio);
        if (targetDistRaw <= 1e-4f)
        {
            outFallbackReason = "target too close";
            return false;
        }
        if (targetDistRaw > maxReach)
        {
            outFallbackReason = "over max reach";
            return false;
        }

        const float minReach = std::fabs(thighLen - calfLen) + 1e-4f;
        const float clampedDist = std::clamp(targetDistRaw, minReach, maxReach - 1e-4f);
        const XMVECTOR targetDir = SafeNormalizeVec(target, XMVectorSet(0, -1, 0, 0));
        const XMVECTOR targetClamped = targetDir * clampedDist;

        XMVECTOR pole = XMVectorSet(0, 0, 1, 0);
        if (kneeHintIdx >= 0 && kneeHintIdx < (int32_t)inOutLocalPose.size())
        {
            const XMFLOAT3 kneeHintF = GetTranslation(inOutLocalPose[(size_t)kneeHintIdx]);
            pole = XMLoadFloat3(&kneeHintF);
            bOutKneeHintUsed = XMVectorGetX(XMVector3LengthSq(pole)) > 1e-6f;
        }

        XMVECTOR poleProjected = pole - targetDir * XMVectorGetX(XMVector3Dot(pole, targetDir));
        if (XMVectorGetX(XMVector3LengthSq(poleProjected)) <= 1e-6f)
        {
            poleProjected = XMVector3Cross(targetDir, XMVectorSet(1, 0, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(poleProjected)) <= 1e-6f)
            {
                poleProjected = XMVector3Cross(targetDir, XMVectorSet(0, 0, 1, 0));
            }
        }
        poleProjected = SafeNormalizeVec(poleProjected, XMVectorSet(0, 0, 1, 0));

        const float x = (clampedDist * clampedDist + thighLen * thighLen - calfLen * calfLen) / (2.0f * clampedDist);
        const float ySq = (std::max)(0.0f, thighLen * thighLen - x * x);
        const float y = std::sqrt(ySq);

        const XMVECTOR kneePos = targetDir * x + poleProjected * y;
        const XMVECTOR desiredUpperDir = SafeNormalizeVec(kneePos, SafeNormalizeVec(upperToKnee, XMVectorSet(0, -1, 0, 0)));
        const XMVECTOR desiredLowerDir = SafeNormalizeVec(targetClamped - kneePos, SafeNormalizeVec(kneeToFoot, XMVectorSet(0, -1, 0, 0)));

        const XMVECTOR qUpper = QuaternionFromTo(upperToKnee, desiredUpperDir);
        const XMVECTOR qLower = QuaternionFromTo(kneeToFoot, desiredLowerDir);

        ApplyQuaternionRotation(inOutLocalPose[(size_t)upperIdx], qUpper, blendAlpha);
        ApplyQuaternionRotation(inOutLocalPose[(size_t)lowerIdx], qLower, blendAlpha);
        AddLocalYOffset(inOutLocalPose[(size_t)footIdx], effectorOffsetY * 0.15f * blendAlpha);

        outReachRatio = clampedDist / (thighLen + calfLen);
        if (targetDistRaw > clampedDist + 1e-3f)
        {
            outQuality = "clamped";
        }
        else if (outReachRatio > 0.97f)
        {
            outQuality = "near-limit";
        }
        else
        {
            outQuality = "stable";
        }
        return true;
    }

    static void UpdateFootLock(FootIKFootLockState& lockState, bool bHasHit, float rawOffsetY, float dt)
    {
        if (!g_Tuning.bEnableFootLock)
        {
            lockState = {};
            return;
        }

        const float vel = (dt > 1e-5f) ? std::fabs(rawOffsetY - lockState.LastRawEffectorOffsetY) / dt : 0.0f;
        lockState.LastRawEffectorOffsetY = rawOffsetY;

        if (!bHasHit)
        {
            lockState.bLocked = false;
            lockState.LockBlendAlpha = 0.0f;
            lockState.StanceTime = 0.0f;
            return;
        }

        const bool bStable = vel <= g_Tuning.FootLockReleaseSpeedThreshold;
        if (!lockState.bLocked)
        {
            lockState.StanceTime = bStable ? (lockState.StanceTime + dt) : 0.0f;
            if (lockState.StanceTime >= g_Tuning.FootLockEnterTime)
            {
                lockState.bLocked = true;
                lockState.LockedEffectorOffsetY = rawOffsetY;
                lockState.LockBlendAlpha = 1.0f;
            }
            return;
        }

        const float drift = std::fabs(rawOffsetY - lockState.LockedEffectorOffsetY);
        if (!bStable || drift > g_Tuning.FootLockReleaseOffsetThreshold)
        {
            lockState.bLocked = false;
            lockState.LockBlendAlpha = 0.0f;
            lockState.StanceTime = 0.0f;
            return;
        }

        lockState.LockedEffectorOffsetY = lockState.LockedEffectorOffsetY + (rawOffsetY - lockState.LockedEffectorOffsetY) * std::clamp(dt * 4.0f, 0.0f, 1.0f);
        lockState.LockBlendAlpha = 1.0f;
    }

    static void ResolveBoneChainIfNeeded(const SkeletalMeshAsset* mesh, FootIKSampleState& ioState)
    {
        if (!mesh || ioState.bBoneChainResolved) return;

        ioState.bBoneChainResolved = true;
        ioState.bUsingProfileBoneChain = false;
        ioState.bUsingHeuristicBoneChainFallback = false;
        ioState.ActiveBoneChainProfileKey.clear();

        ioState.PelvisBoneIndex = -1;
        ioState.LeftUpperLegBoneIndex = -1;
        ioState.LeftLowerLegBoneIndex = -1;
        ioState.LeftFootBoneIndex = -1;
        ioState.LeftToeBoneIndex = -1;
        ioState.LeftHeelBoneIndex = -1;
        ioState.LeftKneeHintBoneIndex = -1;
        ioState.RightUpperLegBoneIndex = -1;
        ioState.RightLowerLegBoneIndex = -1;
        ioState.RightFootBoneIndex = -1;
        ioState.RightToeBoneIndex = -1;
        ioState.RightHeelBoneIndex = -1;
        ioState.RightKneeHintBoneIndex = -1;

        if (const FootIKBoneChainProfileData* profile = FindProfileForMesh(mesh))
        {
            ioState.PelvisBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Pelvis);

            ioState.LeftUpperLegBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Left.Thigh);
            ioState.LeftLowerLegBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Left.Calf);
            ioState.LeftFootBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Left.Foot);
            ioState.LeftToeBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Left.Toe);
            ioState.LeftHeelBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Left.Heel);
            ioState.LeftKneeHintBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Left.KneeHint);

            ioState.RightUpperLegBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Right.Thigh);
            ioState.RightLowerLegBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Right.Calf);
            ioState.RightFootBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Right.Foot);
            ioState.RightToeBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Right.Toe);
            ioState.RightHeelBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Right.Heel);
            ioState.RightKneeHintBoneIndex = FindBoneIndexByNameExactInsensitive(mesh, profile->Right.KneeHint);

            const bool bProfileHasRequired =
                ioState.LeftUpperLegBoneIndex >= 0 && ioState.LeftLowerLegBoneIndex >= 0 && ioState.LeftFootBoneIndex >= 0 &&
                ioState.RightUpperLegBoneIndex >= 0 && ioState.RightLowerLegBoneIndex >= 0 && ioState.RightFootBoneIndex >= 0;
            if (bProfileHasRequired)
            {
                if (ioState.PelvisBoneIndex < 0)
                {
                    ioState.PelvisBoneIndex = FindBoneIndexByTokenInsensitive(mesh, "pelvis", "hip");
                }

                ioState.bUsingProfileBoneChain = true;
                ioState.ActiveBoneChainProfileKey = !profile->Key.empty() ? profile->Key : (!profile->MeshVirtualPath.empty() ? profile->MeshVirtualPath : profile->MeshSourcePath);
                return;
            }
        }

        ioState.bUsingHeuristicBoneChainFallback = true;
        ioState.ActiveBoneChainProfileKey = "heuristic";

        ioState.LeftFootBoneIndex = FindFootBoneIndexFromMesh(mesh, true);
        ioState.RightFootBoneIndex = FindFootBoneIndexFromMesh(mesh, false);
        ioState.LeftLowerLegBoneIndex = FindLegParentIndex(mesh, ioState.LeftFootBoneIndex, "calf", "leg");
        ioState.RightLowerLegBoneIndex = FindLegParentIndex(mesh, ioState.RightFootBoneIndex, "calf", "leg");
        ioState.LeftUpperLegBoneIndex = FindLegParentIndex(mesh, ioState.LeftLowerLegBoneIndex, "thigh", "upleg");
        ioState.RightUpperLegBoneIndex = FindLegParentIndex(mesh, ioState.RightLowerLegBoneIndex, "thigh", "upleg");

        ioState.PelvisBoneIndex = FindBoneIndexByTokenInsensitive(mesh, "pelvis", "hip");
        if (ioState.PelvisBoneIndex < 0 && !mesh->Skeleton.Bones.empty())
        {
            ioState.PelvisBoneIndex = 0;
        }
    }
}

FootIKSampleTuning& FootIKSample::GetTuning()
{
    return g_Tuning;
}

void FootIKSample::Update(SkeletalMeshComponent& component, const std::vector<XMFLOAT4X4>& globalPose, float deltaTime, FootIKSampleState& inOutState)
{
    if (!g_Tuning.bEnableSampling) return;

    const SkeletalMeshAsset* mesh = component.GetMesh();
    World* world = component.GetWorld();
    if (!mesh || !world)
    {
        inOutState.bHasValidBones = false;
        return;
    }

    ResolveBoneChainIfNeeded(mesh, inOutState);
    if (inOutState.LeftFootBoneIndex < 0 || inOutState.RightFootBoneIndex < 0)
    {
        inOutState.bHasValidBones = false;
        return;
    }

    const size_t leftIdx = (size_t)inOutState.LeftFootBoneIndex;
    const size_t rightIdx = (size_t)inOutState.RightFootBoneIndex;
    if (leftIdx >= globalPose.size() || rightIdx >= globalPose.size())
    {
        inOutState.bHasValidBones = false;
        return;
    }

    inOutState.bHasValidBones = true;

    const XMMATRIX componentWorld = component.GetWorldMatrix();

    auto TraceOne = [&](const XMFLOAT3& p, FootIKTraceDebugSample& out)
    {
        out = {};
        out.TraceStart = XMFLOAT3(p.x, p.y + g_Tuning.TraceUpDistance, p.z);
        out.TraceEnd = XMFLOAT3(p.x, p.y - g_Tuning.TraceDownDistance, p.z);

        RaycastHitResult hit{};
        RaycastQueryParams query{};
        query.IgnoreActor = component.GetOwner();
        query.IgnoreComponent = &component;
        if (world->Raycast(out.TraceStart, out.TraceEnd, hit, query) && hit.bBlockingHit)
        {
            out.bHit = true;
            out.HitLocation = hit.Location;
            out.HitNormal = hit.Normal;
            out.VerticalOffset = hit.Location.y - p.y;
        }
    };

    auto TraceFootMulti = [&](int32_t footBoneIndex, FootIKContactDebug& outContact)
    {
        outContact = {};
        const XMFLOAT3 center = GetBoneWorldLocation(globalPose[(size_t)footBoneIndex], componentWorld);
        const XMFLOAT3 forward = GetWorldAxisNormalized(globalPose[(size_t)footBoneIndex], componentWorld, 2);
        const XMFLOAT3 heelPos(center.x - forward.x * g_Tuning.HeelToeSampleDistance,
            center.y - forward.y * g_Tuning.HeelToeSampleDistance,
            center.z - forward.z * g_Tuning.HeelToeSampleDistance);
        const XMFLOAT3 toePos(center.x + forward.x * g_Tuning.HeelToeSampleDistance,
            center.y + forward.y * g_Tuning.HeelToeSampleDistance,
            center.z + forward.z * g_Tuning.HeelToeSampleDistance);

        TraceOne(center, outContact.Center);
        TraceOne(heelPos, outContact.Heel);
        TraceOne(toePos, outContact.Toe);

        int hitCount = 0;
        float offsetSum = 0.0f;
        XMFLOAT3 normalSum(0, 0, 0);
        const FootIKTraceDebugSample* samples[3] = { &outContact.Center, &outContact.Heel, &outContact.Toe };
        for (const FootIKTraceDebugSample* s : samples)
        {
            if (!s->bHit) continue;
            hitCount++;
            offsetSum += s->VerticalOffset;
            normalSum.x += s->HitNormal.x;
            normalSum.y += s->HitNormal.y;
            normalSum.z += s->HitNormal.z;
        }

        outContact.bAnyHit = hitCount > 0;
        if (hitCount > 0)
        {
            outContact.AggregatedOffsetY = offsetSum / (float)hitCount;
            outContact.AggregatedNormal = Normalize3(XMFLOAT3(normalSum.x / (float)hitCount, normalSum.y / (float)hitCount, normalSum.z / (float)hitCount));
        }
    };

    TraceFootMulti(inOutState.LeftFootBoneIndex, inOutState.Left);
    TraceFootMulti(inOutState.RightFootBoneIndex, inOutState.Right);

    const float leftOffset = inOutState.Left.bAnyHit ? inOutState.Left.AggregatedOffsetY : 0.0f;
    const float rightOffset = inOutState.Right.bAnyHit ? inOutState.Right.AggregatedOffsetY : 0.0f;

    const float minOffset = (std::min)(leftOffset, rightOffset);
    inOutState.TargetPelvisOffsetY = (std::clamp)(minOffset, -g_Tuning.MaxPelvisOffsetDown, g_Tuning.MaxPelvisOffsetUp);

    const float pelvisAlpha = std::clamp(deltaTime * g_Tuning.PelvisSmoothingSpeed, 0.0f, 1.0f);
    float pelvisCandidate = inOutState.SmoothedPelvisOffsetY + (inOutState.TargetPelvisOffsetY - inOutState.SmoothedPelvisOffsetY) * pelvisAlpha;
    if (std::fabs(pelvisCandidate - inOutState.SmoothedPelvisOffsetY) < g_Tuning.PelvisJitterDeadzone)
    {
        pelvisCandidate = inOutState.SmoothedPelvisOffsetY;
    }
    inOutState.SmoothedPelvisOffsetY = pelvisCandidate;

    inOutState.LeftRawTargetEffectorOffsetY = std::clamp(leftOffset - inOutState.SmoothedPelvisOffsetY, -g_Tuning.MaxFootOffsetDown, g_Tuning.MaxFootOffsetUp);
    inOutState.RightRawTargetEffectorOffsetY = std::clamp(rightOffset - inOutState.SmoothedPelvisOffsetY, -g_Tuning.MaxFootOffsetDown, g_Tuning.MaxFootOffsetUp);

    UpdateFootLock(inOutState.LeftLock, inOutState.Left.bAnyHit, inOutState.LeftRawTargetEffectorOffsetY, deltaTime);
    UpdateFootLock(inOutState.RightLock, inOutState.Right.bAnyHit, inOutState.RightRawTargetEffectorOffsetY, deltaTime);

    inOutState.LeftTargetEffectorOffsetY = inOutState.LeftLock.bLocked ? inOutState.LeftLock.LockedEffectorOffsetY : inOutState.LeftRawTargetEffectorOffsetY;
    inOutState.RightTargetEffectorOffsetY = inOutState.RightLock.bLocked ? inOutState.RightLock.LockedEffectorOffsetY : inOutState.RightRawTargetEffectorOffsetY;

    const float maxDelta = (std::max)(0.0f, g_Tuning.MaxEffectorDeltaPerSecond) * deltaTime;
    inOutState.LeftTargetEffectorOffsetY = MoveToward(inOutState.LeftAppliedEffectorOffsetY, inOutState.LeftTargetEffectorOffsetY, maxDelta);
    inOutState.RightTargetEffectorOffsetY = MoveToward(inOutState.RightAppliedEffectorOffsetY, inOutState.RightTargetEffectorOffsetY, maxDelta);

    if (std::fabs(inOutState.LeftTargetEffectorOffsetY - inOutState.LeftAppliedEffectorOffsetY) < g_Tuning.EffectorJitterDeadzone)
    {
        inOutState.LeftTargetEffectorOffsetY = inOutState.LeftAppliedEffectorOffsetY;
    }
    if (std::fabs(inOutState.RightTargetEffectorOffsetY - inOutState.RightAppliedEffectorOffsetY) < g_Tuning.EffectorJitterDeadzone)
    {
        inOutState.RightTargetEffectorOffsetY = inOutState.RightAppliedEffectorOffsetY;
    }
}

void FootIKSample::ApplyPoseCorrection(SkeletalMeshComponent& component, std::vector<XMFLOAT4X4>& inOutLocalPose, float deltaTime, FootIKSampleState& inOutState)
{
    if (!g_Tuning.bEnableSampling || !g_Tuning.bApplyRuntimeCorrection)
    {
        inOutState.AppliedPelvisOffsetY = 0.0f;
        inOutState.LeftAppliedEffectorOffsetY = 0.0f;
        inOutState.RightAppliedEffectorOffsetY = 0.0f;
        inOutState.bLeftOrientationApplied = false;
        inOutState.bRightOrientationApplied = false;
        inOutState.bLeftSolverUsed = false;
        inOutState.bRightSolverUsed = false;
        inOutState.bLeftSolverFallbackUsed = false;
        inOutState.bRightSolverFallbackUsed = false;
        inOutState.bLeftKneeHintUsed = false;
        inOutState.bRightKneeHintUsed = false;
        inOutState.bLeftToeHeelProfileUsed = false;
        inOutState.bRightToeHeelProfileUsed = false;
        inOutState.LeftSolverReachRatio = 0.0f;
        inOutState.RightSolverReachRatio = 0.0f;
        inOutState.LeftSolverQualityState.clear();
        inOutState.RightSolverQualityState.clear();
        inOutState.LeftSolverFallbackReason.clear();
        inOutState.RightSolverFallbackReason.clear();
        return;
    }

    if (!inOutState.bHasValidBones || inOutLocalPose.empty()) return;

    const SkeletalMeshAsset* mesh = component.GetMesh();
    if (!mesh) return;

    int32_t pelvisBoneIndex = inOutState.PelvisBoneIndex;
    if (pelvisBoneIndex < 0)
    {
        pelvisBoneIndex = mesh->Skeleton.Bones.empty() ? -1 : 0;
    }
    if (pelvisBoneIndex >= 0 && pelvisBoneIndex < (int32_t)inOutLocalPose.size())
    {
        inOutState.AppliedPelvisOffsetY = inOutState.SmoothedPelvisOffsetY;
        AddLocalYOffset(inOutLocalPose[(size_t)pelvisBoneIndex], inOutState.AppliedPelvisOffsetY);
    }

    const float effAlpha = std::clamp(deltaTime * g_Tuning.EffectorSmoothingSpeed, 0.0f, 1.0f);
    inOutState.LeftAppliedEffectorOffsetY = inOutState.LeftAppliedEffectorOffsetY + (inOutState.LeftTargetEffectorOffsetY - inOutState.LeftAppliedEffectorOffsetY) * effAlpha;
    inOutState.RightAppliedEffectorOffsetY = inOutState.RightAppliedEffectorOffsetY + (inOutState.RightTargetEffectorOffsetY - inOutState.RightAppliedEffectorOffsetY) * effAlpha;

    auto ApplyFallbackDistribution = [&](int32_t upperIdx, int32_t lowerIdx, int32_t footIdx, float effectorOffsetY)
    {
        const float upperW = (std::max)(0.0f, g_Tuning.UpperLegInfluence);
        const float lowerW = (std::max)(0.0f, g_Tuning.LowerLegInfluence);
        const float footW = (std::max)(0.0f, g_Tuning.FootInfluence);
        const float sumW = upperW + lowerW + footW;
        if (sumW <= 1e-5f) return;

        if (upperIdx >= 0 && upperIdx < (int32_t)inOutLocalPose.size()) AddLocalYOffset(inOutLocalPose[(size_t)upperIdx], effectorOffsetY * (upperW / sumW));
        if (lowerIdx >= 0 && lowerIdx < (int32_t)inOutLocalPose.size()) AddLocalYOffset(inOutLocalPose[(size_t)lowerIdx], effectorOffsetY * (lowerW / sumW));
        if (footIdx >= 0 && footIdx < (int32_t)inOutLocalPose.size()) AddLocalYOffset(inOutLocalPose[(size_t)footIdx], effectorOffsetY * (footW / sumW));
    };

    auto ApplyLeg = [&](int32_t upperIdx, int32_t lowerIdx, int32_t footIdx, int32_t kneeHintIdx, int32_t toeIdx, int32_t heelIdx, float effectorOffsetY, const FootIKContactDebug& contact,
                        XMFLOAT3& outNormal, bool& bOutOrient, bool& bOutSolverUsed, bool& bOutSolverFallback,
                        bool& bOutKneeHintUsed, bool& bOutToeHeelUsed, float& outReachRatio, std::string& outQuality, std::string& outFallbackReason)
    {
        bOutOrient = false;
        outNormal = XMFLOAT3(0, 1, 0);
        bOutSolverUsed = false;
        bOutSolverFallback = false;
        bOutKneeHintUsed = false;
        bOutToeHeelUsed = false;
        outReachRatio = 0.0f;
        outQuality = "off";
        outFallbackReason.clear();

        if (footIdx < 0 || footIdx >= (int32_t)inOutLocalPose.size())
        {
            outFallbackReason = "invalid foot";
            outQuality = "invalid";
            return;
        }

        if (g_Tuning.bEnableTwoBoneSolver)
        {
            bOutSolverUsed = TryApplyTwoBoneIKVertical(
                inOutLocalPose,
                upperIdx,
                lowerIdx,
                footIdx,
                kneeHintIdx,
                toeIdx,
                heelIdx,
                effectorOffsetY,
                g_Tuning.TwoBoneBlendAlpha,
                g_Tuning.TwoBoneMaxStretchRatio,
                bOutKneeHintUsed,
                bOutToeHeelUsed,
                outReachRatio,
                outQuality,
                outFallbackReason);
        }
        else
        {
            outFallbackReason = "solver disabled";
        }

        if (!bOutSolverUsed)
        {
            ApplyFallbackDistribution(upperIdx, lowerIdx, footIdx, effectorOffsetY);
            bOutSolverFallback = true;
            if (outQuality == "off") outQuality = "fallback";
        }

        if (g_Tuning.bEnableFootOrientation && contact.bAnyHit)
        {
            outNormal = Normalize3(Lerp3(outNormal, contact.AggregatedNormal, 1.0f));
            bOutOrient = true;
            AlignFootToNormalLocalApprox(inOutLocalPose[(size_t)footIdx], outNormal, g_Tuning.FootNormalAlignStrength, g_Tuning.MaxNormalTiltDegrees);
        }
    };

    ApplyLeg(inOutState.LeftUpperLegBoneIndex, inOutState.LeftLowerLegBoneIndex, inOutState.LeftFootBoneIndex,
        inOutState.LeftKneeHintBoneIndex, inOutState.LeftToeBoneIndex, inOutState.LeftHeelBoneIndex,
        inOutState.LeftAppliedEffectorOffsetY, inOutState.Left, inOutState.LeftAppliedNormal, inOutState.bLeftOrientationApplied,
        inOutState.bLeftSolverUsed, inOutState.bLeftSolverFallbackUsed,
        inOutState.bLeftKneeHintUsed, inOutState.bLeftToeHeelProfileUsed, inOutState.LeftSolverReachRatio, inOutState.LeftSolverQualityState, inOutState.LeftSolverFallbackReason);

    ApplyLeg(inOutState.RightUpperLegBoneIndex, inOutState.RightLowerLegBoneIndex, inOutState.RightFootBoneIndex,
        inOutState.RightKneeHintBoneIndex, inOutState.RightToeBoneIndex, inOutState.RightHeelBoneIndex,
        inOutState.RightAppliedEffectorOffsetY, inOutState.Right, inOutState.RightAppliedNormal, inOutState.bRightOrientationApplied,
        inOutState.bRightSolverUsed, inOutState.bRightSolverFallbackUsed,
        inOutState.bRightKneeHintUsed, inOutState.bRightToeHeelProfileUsed, inOutState.RightSolverReachRatio, inOutState.RightSolverQualityState, inOutState.RightSolverFallbackReason);
}

int32_t FootIKSample::FindFootBoneIndex(const SkeletalMeshComponent& component, bool bLeftFoot)
{
    return FindFootBoneIndexFromMesh(component.GetMesh(), bLeftFoot);
}
