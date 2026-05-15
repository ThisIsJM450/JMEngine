#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

class SkeletalMeshComponent;

struct FootIKTraceDebugSample
{
    DirectX::XMFLOAT3 TraceStart{0, 0, 0};
    DirectX::XMFLOAT3 TraceEnd{0, 0, 0};
    bool bHit = false;
    DirectX::XMFLOAT3 HitLocation{0, 0, 0};
    DirectX::XMFLOAT3 HitNormal{0, 1, 0};
    float VerticalOffset = 0.0f;
};

struct FootIKContactDebug
{
    FootIKTraceDebugSample Center;
    FootIKTraceDebugSample Heel;
    FootIKTraceDebugSample Toe;
    bool bAnyHit = false;
    float AggregatedOffsetY = 0.0f;
    DirectX::XMFLOAT3 AggregatedNormal{0, 1, 0};
};

struct FootIKFootLockState
{
    bool bLocked = false;
    float LockedEffectorOffsetY = 0.0f;
    float LockBlendAlpha = 0.0f;
    float StanceTime = 0.0f;
    float LastRawEffectorOffsetY = 0.0f;
};

struct FootIKSampleState
{
    bool bHasValidBones = false;
    bool bBoneChainResolved = false;
    bool bUsingProfileBoneChain = false;
    bool bUsingHeuristicBoneChainFallback = false;
    std::string ActiveBoneChainProfileKey;

    int32_t PelvisBoneIndex = -1;

    int32_t LeftFootBoneIndex = -1;
    int32_t RightFootBoneIndex = -1;

    int32_t LeftLowerLegBoneIndex = -1;
    int32_t RightLowerLegBoneIndex = -1;
    int32_t LeftUpperLegBoneIndex = -1;
    int32_t RightUpperLegBoneIndex = -1;

    int32_t LeftToeBoneIndex = -1;
    int32_t RightToeBoneIndex = -1;
    int32_t LeftHeelBoneIndex = -1;
    int32_t RightHeelBoneIndex = -1;
    int32_t LeftKneeHintBoneIndex = -1;
    int32_t RightKneeHintBoneIndex = -1;

    FootIKContactDebug Left;
    FootIKContactDebug Right;

    FootIKFootLockState LeftLock;
    FootIKFootLockState RightLock;

    float TargetPelvisOffsetY = 0.0f;
    float SmoothedPelvisOffsetY = 0.0f;
    float AppliedPelvisOffsetY = 0.0f;

    float LeftRawTargetEffectorOffsetY = 0.0f;
    float RightRawTargetEffectorOffsetY = 0.0f;
    float LeftTargetEffectorOffsetY = 0.0f;
    float RightTargetEffectorOffsetY = 0.0f;
    float LeftAppliedEffectorOffsetY = 0.0f;
    float RightAppliedEffectorOffsetY = 0.0f;

    DirectX::XMFLOAT3 LeftAppliedNormal{0, 1, 0};
    DirectX::XMFLOAT3 RightAppliedNormal{0, 1, 0};
    bool bLeftOrientationApplied = false;
    bool bRightOrientationApplied = false;

    bool bLeftSolverUsed = false;
    bool bRightSolverUsed = false;
    bool bLeftSolverFallbackUsed = false;
    bool bRightSolverFallbackUsed = false;
    bool bLeftKneeHintUsed = false;
    bool bRightKneeHintUsed = false;
    bool bLeftToeHeelProfileUsed = false;
    bool bRightToeHeelProfileUsed = false;
    float LeftSolverReachRatio = 0.0f;
    float RightSolverReachRatio = 0.0f;
    std::string LeftSolverQualityState;
    std::string RightSolverQualityState;
    std::string LeftSolverFallbackReason;
    std::string RightSolverFallbackReason;
};

struct FootIKSampleTuning
{
    bool bEnableSampling = true;
    bool bApplyRuntimeCorrection = true;
    bool bEnableFootOrientation = true;

    bool bEnableFootLock = true;
    bool bEnableTwoBoneSolver = true;

    float TraceUpDistance = 25.0f;
    float TraceDownDistance = 55.0f;
    float HeelToeSampleDistance = 8.0f;

    float MaxPelvisOffsetDown = 35.0f;
    float MaxPelvisOffsetUp = 10.0f;
    float PelvisSmoothingSpeed = 14.0f;
    float PelvisJitterDeadzone = 0.15f;

    float MaxFootOffsetUp = 20.0f;
    float MaxFootOffsetDown = 25.0f;
    float EffectorSmoothingSpeed = 20.0f;
    float EffectorJitterDeadzone = 0.1f;
    float MaxEffectorDeltaPerSecond = 120.0f;

    float FootLockEnterTime = 0.08f;
    float FootLockReleaseSpeedThreshold = 6.0f;
    float FootLockReleaseOffsetThreshold = 6.0f;

    float UpperLegInfluence = 0.30f;
    float LowerLegInfluence = 0.45f;
    float FootInfluence = 0.25f;

    float FootNormalAlignStrength = 0.65f;
    float MaxNormalTiltDegrees = 28.0f;

    float TwoBoneMaxStretchRatio = 1.04f;
    float TwoBoneBlendAlpha = 0.85f;
};

class FootIKSample
{
public:
    static FootIKSampleTuning& GetTuning();

    static void Update(
        SkeletalMeshComponent& component,
        const std::vector<DirectX::XMFLOAT4X4>& globalPose,
        float deltaTime,
        FootIKSampleState& inOutState);

    static void ApplyPoseCorrection(
        SkeletalMeshComponent& component,
        std::vector<DirectX::XMFLOAT4X4>& inOutLocalPose,
        float deltaTime,
        FootIKSampleState& inOutState);

private:
    static int32_t FindFootBoneIndex(const SkeletalMeshComponent& component, bool bLeftFoot);
};