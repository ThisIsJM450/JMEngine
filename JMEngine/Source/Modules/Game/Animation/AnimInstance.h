#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

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
    //const std::vector<DirectX::XMFLOAT4X4> GetAnimRefPose() const;
    
protected:
    void SampleSectionToLocalPose(const AnimSection& sec, float tSec);

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
};


