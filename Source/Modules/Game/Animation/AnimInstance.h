#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

#include "../../Core/Transform.h"

struct AnimSection;
struct Skeleton;
struct AnimSequenceAsset;
class SkeletalMeshAsset;

class AnimInstance
{
public:
    void Initialize(const std::shared_ptr<SkeletalMeshAsset>& mesh);
    void Tick(float dt);
    
    void SetSequence(const std::shared_ptr<AnimSequenceAsset> seq);
    void Play(const std::string& sectionName = "Default", bool bLoop = true, float playRate = 1.f);
    void Stop();
    void EvaluateSequence(float dt);
    const std::vector<DirectX::XMFLOAT4X4>& GetLocalPose() const { return m_LocalPose; }
    
    // RootMotion
    bool HasExtractedRootMotion() const { return m_bHasRootDelta; }
    Transform ConsumeExtractedRootMotion();

    bool IsCurrentSectionRootMotionEnabled() const;
    bool IsCurrentSectionFromMontage() const;
    const std::shared_ptr<AnimSequenceAsset>& GetSequence() const { return m_Seq; }
    bool IsPlaying() const { return m_bPlaying; }
    bool IsLooping() const { return m_bLoop; }
    float GetPlayRate() const { return m_PlayRate; }
    float GetTimeInSection() const { return m_TimeInSection; }
    int GetSectionIndex() const { return m_SectionIndex; }
    std::string GetCurrentSectionName() const;

    void SetRootBoneIndex(int32_t InRootBoneIndex) { m_RootBoneIndex = InRootBoneIndex; }
    void SetConsumeRootInPose(bool bConsume) { m_bConsumeRootInPose = bConsume; }
    bool GetConsumeRootInPose() const { return m_bConsumeRootInPose; }
    void SetRootMotionExtractionEnabled(bool bEnable);
    bool IsRootMotionExtractionEnabled() const { return m_bEnableRootMotionExtraction; }
    
protected:
    void SampleSectionToLocalPose(const AnimSection& sec, float tSec);
    
    Transform SampleBoneTransform(const AnimSection& sec, uint32_t boneIdx, float tSec) const;
    int32_t FindBoneIndexFromNodeIndex(int32_t nodeIdx) const;
    DirectX::XMMATRIX SampleNodeLocalMatrix(const AnimSection& sec, int32_t nodeIdx, float tSec) const;
    DirectX::XMMATRIX SampleNodeGlobalMatrix(const AnimSection& sec, int32_t nodeIdx, float tSec) const;
    Transform SampleRootMotionBasisTransform(const AnimSection& sec, float tSec) const;
    Transform SampleRootMotionLockedBasisTransform(const AnimSection& sec, float tSec) const;
    Transform ExtractRootDeltaRange(const AnimSection& sec, float tA, float tB) const;
    Transform ExtractRootDeltaLoopAware(const AnimSection& sec, float t0, float t1, bool bWrapped) const;
    void ApplyRootLockToLocalPose(const AnimSection& sec);

private:
    Skeleton* m_Skeleton = nullptr;
    std::weak_ptr<SkeletalMeshAsset> m_Mesh;
    float m_Time = 0.f;
    std::vector<DirectX::XMFLOAT4X4> m_LocalPose;
    
    std::shared_ptr<AnimSequenceAsset> m_Seq;
    uint32_t m_BoneCount = 0;
    bool  m_bPlaying = false;
    bool  m_bLoop = true;
    float m_PlayRate = 1.f;

    int   m_SectionIndex = 0;
    float m_TimeInSection = 0.f;
    
    // RootMotion
    int32_t m_RootBoneIndex = 0;

    Transform m_ExtractedRootDelta;
    bool m_bHasRootDelta = false;

    bool m_bConsumeRootInPose = true;
    bool m_bEnableRootMotionExtraction = true;
    bool m_bSkipRootMotionThisTick = false;
    bool m_bHasFirstRootInSection = false;
    Transform m_FirstRootInSection;
};


