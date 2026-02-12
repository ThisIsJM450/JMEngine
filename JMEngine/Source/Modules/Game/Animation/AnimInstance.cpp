#include "AnimInstance.h"

#include <algorithm>
#include <iostream>

#include "AnimSequence.h"
#include "../Skeletal/SkeletalMeshData.h"

void AnimInstance::Initialize(const std::shared_ptr<SkeletalMeshAsset>& mesh)
{
    if (mesh)
    {
        m_Skeleton = &mesh->Skeleton;
        m_BoneCount = (uint32_t)m_Skeleton->Bones.size();
    }
    m_Mesh = mesh;
    const auto& skel = mesh->Skeleton;
    m_LocalPose = skel.RefLocalPose; // 기본 포즈로 시작
    m_Time = 0.f;
}

void AnimInstance::Tick(float dt)
{
    m_Time += dt;
    if (m_Skeleton == nullptr)
    {
        return;
    }

    EvaluateSequence(dt);
}

void AnimInstance::EvaluateSequence(float dt)
{
    if (!m_bPlaying || !m_Seq || !m_Skeleton) return;
    if (m_Seq->Sections.empty()) return;

    const AnimSection& sec = m_Seq->Sections[m_SectionIndex];

    // 진행
    m_TimeInSection += dt * m_PlayRate;

    // 루프/클램프
    if (sec.LengthSec > 1e-6f)
    {
        if (m_bLoop)
        {
            // fmod 안정(음수 방지)
            while (m_TimeInSection >= sec.LengthSec) m_TimeInSection -= sec.LengthSec;
            while (m_TimeInSection < 0.f)            m_TimeInSection += sec.LengthSec;
        }
        else
        {
            if (m_TimeInSection >= sec.LengthSec)
            {
                m_TimeInSection = sec.LengthSec;
                m_bPlaying = false; // 끝나면 자동 정지(원하면 유지)
            }
            if (m_TimeInSection < 0.f) m_TimeInSection = 0.f;
        }
    }

    // 샘플링 결과로 LocalPose 업데이트
    SampleSectionToLocalPose(sec, m_TimeInSection);
}

/*
const std::vector<DirectX::XMFLOAT4X4> AnimInstance::GetAnimRefPose() const
{
    if (!m_Skeleton)
    {
        return std::vector<DirectX::XMFLOAT4X4>();
    }
    const auto& Nodes = m_Skeleton->Nodes;
    std::vector<DirectX::XMFLOAT4X4> nodeLocal(Nodes.size());
    for (size_t ni = 0; ni < Nodes.size(); ++ni)
    {
        nodeLocal[ni] = Nodes[ni].RefLocal;
    }
    if (!m_bPlaying || !m_Seq)
    {
        // 1) nodeLocal: RefLocal로 시작 

        return nodeLocal;
    }
    const AnimSection& sec = m_Seq->Sections[m_SectionIndex];
    for (size_t bi = 0; bi < m_Skeleton->Bones.size(); ++bi)
    {
        XMStoreFloat4x4(&nodeLocal[m_Skeleton->BoneToNode[bi]], sec.RefPose[bi].ToMatrix());
    }
    return nodeLocal;
}
*/

void AnimInstance::SampleSectionToLocalPose(const AnimSection& sec, float tSec)
{
    // 기본은 RefPose
    /*
    m_LocalPose = m_Skeleton->RefLocalPose;
    if (m_LocalPose.size() != m_BoneCount)
    {
        m_LocalPose.resize(m_BoneCount);
    }
*/
    
    if (m_BoneCount == 0 || sec.NumFrames == 0) return;
    if (sec.Keys.size() != (size_t)sec.NumFrames * m_BoneCount) return;

    // frame0, frame1, alpha
    float clamped = tSec;
    if (sec.LengthSec > 1e-6f)
    {
        clamped = std::clamp(clamped, 0.f, sec.LengthSec);
    }

    const float frameF = clamped * sec.SampleRate;
    uint32_t f0 = (uint32_t)std::floor(frameF);
    uint32_t f1 = std::min(f0 + 1, sec.NumFrames - 1);
    float alpha = frameF - (float)f0;
    alpha = std::clamp(alpha, 0.f, 1.f);

    // 본별 Transform 보간 -> 행렬로 변환하여 LocalPose에 저장
    for (uint32_t b = 0; b < m_BoneCount; ++b)
    {
        const Transform& k0 = sec.Keys[(size_t)f0 * m_BoneCount + b];
        const Transform& k1 = sec.Keys[(size_t)f1 * m_BoneCount + b];

        Transform blended = Transform::Lerp(k0, k1, alpha);

        DirectX::XMFLOAT4X4 M;
        DirectX::XMStoreFloat4x4(&M, XMMatrixTranspose(blended.ToMatrix()));
        m_LocalPose[b] = M;
    }
}

void AnimInstance::SetSequence(const std::shared_ptr<AnimSequenceAsset> seq)
{
    m_Seq = seq;
    m_SectionIndex = 0;
    m_TimeInSection = 0.f;
    m_bPlaying = false;
}

void AnimInstance::Play(const std::string& sectionName, bool bLoop, float playRate)
{
    if (m_Seq == nullptr)
    {
        return;
    }
    m_bLoop = bLoop;
    m_PlayRate = playRate;
    m_bPlaying = true;

    int idx = -1;
    for (int i = 0; i < (int)m_Seq->Sections.size(); ++i)
    {
        if (m_Seq->Sections[i].Name == sectionName)
        {
            idx = i; break;
        }
    }

    m_SectionIndex = (idx >= 0) ? idx : 0;
    m_TimeInSection = 0.f;
}

void AnimInstance::Stop()
{
    m_bPlaying = false;
    m_TimeInSection = 0.f;

    // 멈추면 RefPose로 복귀(원하면 유지로 바꿔도 됨)
    if (m_Skeleton)
    {
        m_LocalPose = m_Skeleton->RefLocalPose;
    }
}
