#include "AnimInstance.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "AnimSequence.h"
#include "../Skeletal/SkeletalMeshData.h"

namespace
{
    std::string ToLowerAscii(std::string s)
    {
        for (char& c : s)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return s;
    }

    int32_t ChooseRootBoneIndexFromSkeleton(const Skeleton& skel)
    {
        if (skel.Bones.empty())
        {
            return 0;
        }

        // 1) 이름에 root가 포함된 본 우선
        for (int32_t i = 0; i < static_cast<int32_t>(skel.Bones.size()); ++i)
        {
            const std::string lower = ToLowerAscii(skel.Bones[i].Name);
            if (lower.find("root") != std::string::npos)
            {
                return i;
            }
        }

        // 2) 계층상 최상위 deform bone(parent == -1)
        for (int32_t i = 0; i < static_cast<int32_t>(skel.Bones.size()); ++i)
        {
            if (skel.Bones[i].ParentIndex < 0)
            {
                return i;
            }
        }

        return 0;
    }

    bool IsNearlyIdentityDelta(const Transform& delta, float posTol, float rotTolDeg)
    {
        using namespace DirectX;

        const float px = delta.m_Pos.x;
        const float py = delta.m_Pos.y;
        const float pz = delta.m_Pos.z;
        const float posLen = std::sqrt(px * px + py * py + pz * pz);
        if (posLen > posTol)
        {
            return false;
        }

        XMFLOAT4 q{};
        XMStoreFloat4(&q, XMQuaternionNormalize(delta.GetRotationQuat()));
        float dot = q.w; // identity quat(0,0,0,1)와의 dot
        dot = std::min(1.0f, std::max(-1.0f, std::fabs(dot)));
        const float angDeg = XMConvertToDegrees(2.0f * std::acos(dot));
        return angDeg <= rotTolDeg;
    }
}

static Transform TransformFromMatrixSafe(DirectX::FXMMATRIX M)
{
    using namespace DirectX;

    Transform out;

    XMVECTOR S, R, T;
    if (!XMMatrixDecompose(&S, &R, &T, M))
    {
        out.SetPosition(0.f, 0.f, 0.f);
        out.SetScale(1.f, 1.f, 1.f);
        XMFLOAT4 idq(0.f, 0.f, 0.f, 1.f);
        out.SetRotationQuat(idq);
        return out;
    }

    XMFLOAT3 pos, scl;
    XMStoreFloat3(&pos, T);
    XMStoreFloat3(&scl, S);

    R = XMQuaternionNormalize(R);
    XMFLOAT4 q;
    XMStoreFloat4(&q, R);

    out.SetPosition(pos.x, pos.y, pos.z);
    out.SetScale(scl.x, scl.y, scl.z);
    out.SetRotationQuat(q);
    return out;
}

static Transform TransformFromRigidMatrix(DirectX::FXMMATRIX M)
{
    using namespace DirectX;

    XMFLOAT4X4 m{};
    XMStoreFloat4x4(&m, M);

    const XMVECTOR axisX0 = XMVectorSet(m._11, m._12, m._13, 0.f);
    const XMVECTOR axisY0 = XMVectorSet(m._21, m._22, m._23, 0.f);

    XMVECTOR axisX = XMVector3Normalize(axisX0);
    XMVECTOR axisY = axisY0 - XMVectorScale(axisX, XMVectorGetX(XMVector3Dot(axisY0, axisX)));
    axisY = XMVector3Normalize(axisY);
    XMVECTOR axisZ = XMVector3Normalize(XMVector3Cross(axisX, axisY));

    XMMATRIX R = XMMatrixIdentity();
    R.r[0] = XMVectorSelect(R.r[0], axisX, XMVectorSelectControl(1, 1, 1, 0));
    R.r[1] = XMVectorSelect(R.r[1], axisY, XMVectorSelectControl(1, 1, 1, 0));
    R.r[2] = XMVectorSelect(R.r[2], axisZ, XMVectorSelectControl(1, 1, 1, 0));

    XMFLOAT4 q{};
    XMStoreFloat4(&q, XMQuaternionNormalize(XMQuaternionRotationMatrix(R)));

    Transform out;
    out.SetPosition(m._41, m._42, m._43);
    out.SetRotationQuat(q);
    out.SetScale(1.f, 1.f, 1.f);
    return out;
}

#if defined(_DEBUG)
static void DebugValidateRootDelta(
    const Transform& root0,
    const Transform& root1,
    const Transform& delta)
{
    using namespace DirectX;

    const XMMATRIX reconM = root0.ToMatrix() * delta.ToMatrix();
    const Transform recon = TransformFromMatrixSafe(reconM);

    const float dx = recon.m_Pos.x - root1.m_Pos.x;
    const float dy = recon.m_Pos.y - root1.m_Pos.y;
    const float dz = recon.m_Pos.z - root1.m_Pos.z;
    const float posErr = std::sqrt(dx * dx + dy * dy + dz * dz);

    XMFLOAT4 qA{}, qB{};
    XMStoreFloat4(&qA, XMQuaternionNormalize(recon.GetRotationQuat()));
    XMStoreFloat4(&qB, XMQuaternionNormalize(root1.GetRotationQuat()));
    float dot = qA.x * qB.x + qA.y * qB.y + qA.z * qB.z + qA.w * qB.w;
    dot = std::min(1.0f, std::max(-1.0f, std::fabs(dot)));
    const float angErr = 2.0f * std::acos(dot);

    static int sWarnCount = 0;
    if (sWarnCount < 30 && (posErr > 0.01f || angErr > XMConvertToRadians(0.5f)))
    {
        ++sWarnCount;
        std::cout << "[RootMotion][Validate] posErr=" << posErr
                  << " angErrDeg=" << XMConvertToDegrees(angErr) << std::endl;
    }
}
#endif

void AnimInstance::Initialize(const std::shared_ptr<SkeletalMeshAsset>& mesh)
{
    if (mesh)
    {
        m_Skeleton = &mesh->Skeleton;
        m_BoneCount = (uint32_t)m_Skeleton->Bones.size();
        m_RootBoneIndex = ChooseRootBoneIndexFromSkeleton(*m_Skeleton);

#if defined(_DEBUG)
        if (m_RootBoneIndex >= 0 && static_cast<uint32_t>(m_RootBoneIndex) < m_BoneCount)
        {
            std::cout << "[RootMotion][Init] RootBoneIndex=" << m_RootBoneIndex
                      << " Name=" << m_Skeleton->Bones[static_cast<size_t>(m_RootBoneIndex)].Name
                      << std::endl;
        }
#endif
    }
    m_Mesh = mesh;
    const auto& skel = mesh->Skeleton;
    m_LocalPose = skel.RefLocalPose; // 기본 포즈로 시작
    m_Time = 0.f;
    
    m_bHasRootDelta = false;
    m_bHasFirstRootInSection = false;
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

    const float t0 = m_TimeInSection;
    float nextTime = m_TimeInSection + dt * m_PlayRate;

    bool bWrapped = false;

    if (sec.LengthSec > 1e-6f)
    {
        if (m_bLoop)
        {
            while (nextTime >= sec.LengthSec) { nextTime -= sec.LengthSec; bWrapped = true; }
            while (nextTime < 0.f)            { nextTime += sec.LengthSec; bWrapped = true; }
        }
        else
        {
            if (nextTime >= sec.LengthSec)
            {
                nextTime = sec.LengthSec;
                m_bPlaying = false;
            }
            if (nextTime < 0.f) nextTime = 0.f;
        }
    }

    m_TimeInSection = nextTime;

    // 1) pose 샘플링
    SampleSectionToLocalPose(sec, m_TimeInSection);

    // 재생/섹션 시작 직후 첫 평가 틱은 델타 적용을 스킵해서 시작 팝을 방지한다.
    if (m_bSkipRootMotionThisTick)
    {
        m_bSkipRootMotionThisTick = false;
        m_bHasRootDelta = false;
        m_ExtractedRootDelta = Transform();

        if (sec.bEnableRootMotion && m_bEnableRootMotionExtraction && m_bConsumeRootInPose)
        {
            // 첫 유효 프레임에서 lock 전/후 기준 차이를 1회 보정해서
            // RootMotion ON/OFF 절대 기준 오프셋을 제거한다.
            const Transform unlocked = SampleRootMotionBasisTransform(sec, m_TimeInSection);
            const Transform locked = SampleRootMotionLockedBasisTransform(sec, m_TimeInSection);

            DirectX::XMVECTOR det;
            const DirectX::XMMATRIX invLocked = DirectX::XMMatrixInverse(&det, locked.ToMatrix());
            const DirectX::XMMATRIX comp = invLocked * unlocked.ToMatrix();
            m_ExtractedRootDelta = TransformFromRigidMatrix(comp);
            m_ExtractedRootDelta.SetScale(1.f, 1.f, 1.f);
            m_bHasRootDelta = true;

            ApplyRootLockToLocalPose(sec);
        }
        return;
    }

    // 2) root motion 추출
    m_bHasRootDelta = false;
    if (m_bEnableRootMotionExtraction && sec.bEnableRootMotion && m_BoneCount > 0 && m_RootBoneIndex >= 0 && (uint32_t)m_RootBoneIndex < m_BoneCount)
    {
        m_ExtractedRootDelta = ExtractRootDeltaLoopAware(sec, t0, m_TimeInSection, bWrapped);
        m_bHasRootDelta = true;
#if defined(_DEBUG)
        const Transform root0 = SampleRootMotionBasisTransform(sec, t0);
        const Transform root1 = SampleRootMotionBasisTransform(sec, m_TimeInSection);
        DebugValidateRootDelta(root0, root1, m_ExtractedRootDelta);
#endif

        if (m_bConsumeRootInPose)
        {
            ApplyRootLockToLocalPose(sec);
        }
    }
}

void AnimInstance::SetRootMotionExtractionEnabled(bool bEnable)
{
    m_bEnableRootMotionExtraction = bEnable;
    if (!m_bEnableRootMotionExtraction)
    {
        m_bHasRootDelta = false;
        m_ExtractedRootDelta = Transform();
    }
}

Transform AnimInstance::ConsumeExtractedRootMotion()
{
    Transform out = m_ExtractedRootDelta;
    m_bHasRootDelta = false;

    // 남는 값 초기화(선택)
    out.SetScale(1.f, 1.f, 1.f);
    return out;
}

bool AnimInstance::IsCurrentSectionRootMotionEnabled() const
{
    if (!m_Seq) 
        return false;
    if (m_SectionIndex < 0 || m_SectionIndex >= (int)m_Seq->Sections.size()) 
        return false;
    return m_Seq->Sections[m_SectionIndex].bEnableRootMotion;
}

bool AnimInstance::IsCurrentSectionFromMontage() const
{
    if (!m_Seq) 
        return false;
    if (m_SectionIndex < 0 || m_SectionIndex >= (int)m_Seq->Sections.size()) 
        return false;
    return m_Seq->Sections[m_SectionIndex].bFromMontage;
}

std::string AnimInstance::GetCurrentSectionName() const
{
    if (!m_Seq)
    {
        return "Default";
    }
    if (m_SectionIndex < 0 || m_SectionIndex >= static_cast<int>(m_Seq->Sections.size()))
    {
        return "Default";
    }
    const std::string& name = m_Seq->Sections[static_cast<size_t>(m_SectionIndex)].Name;
    return name.empty() ? "Default" : name;
}

void AnimInstance::SampleSectionToLocalPose(const AnimSection& sec, float tSec)
{
    if (m_BoneCount == 0 || sec.NumFrames == 0) return;
    if (sec.Keys.size() != (size_t)sec.NumFrames * m_BoneCount) return;

    // frame0, frame1, alpha
    float clamped = tSec;
    if (sec.LengthSec > 1e-6f)
    {
        clamped = std::clamp(clamped, 0.f, sec.LengthSec);
    }

    const float maxFrameF = static_cast<float>(sec.NumFrames - 1);
    const float frameF = std::clamp(clamped * sec.SampleRate, 0.0f, maxFrameF);
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
        DirectX::XMStoreFloat4x4(&M, blended.ToMatrix());
        m_LocalPose[b] = M;
    }
}

Transform AnimInstance::SampleBoneTransform(const AnimSection& sec, uint32_t boneIdx, float tSec) const
{
    Transform identity;

    if (sec.NumFrames == 0 || m_BoneCount == 0) 
        return identity;
    if (boneIdx >= m_BoneCount) 
        return identity;
    if (sec.Keys.size() != (size_t)sec.NumFrames * m_BoneCount) 
        return identity;

    float clamped = tSec;
    if (sec.LengthSec > 1e-6f)
        clamped = std::clamp(clamped, 0.f, sec.LengthSec);

    const float maxFrameF = static_cast<float>(sec.NumFrames - 1);
    const float frameF = std::clamp(clamped * sec.SampleRate, 0.0f, maxFrameF);
    const uint32_t f0 = (uint32_t)std::floor(frameF);
    const uint32_t f1 = std::min(f0 + 1, sec.NumFrames - 1);
    float alpha = frameF - (float)f0;
    alpha = std::clamp(alpha, 0.f, 1.f);

    const Transform& k0 = sec.Keys[(size_t)f0 * m_BoneCount + boneIdx];
    const Transform& k1 = sec.Keys[(size_t)f1 * m_BoneCount + boneIdx];

    return Transform::Lerp(k0, k1, alpha);
}

int32_t AnimInstance::FindBoneIndexFromNodeIndex(int32_t nodeIdx) const
{
    if (!m_Skeleton || nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= m_Skeleton->Nodes.size())
    {
        return -1;
    }

    const std::string& nodeName = m_Skeleton->Nodes[static_cast<size_t>(nodeIdx)].Name;
    auto it = m_Skeleton->BoneNameToIndex.find(nodeName);
    if (it == m_Skeleton->BoneNameToIndex.end())
    {
        return -1;
    }

    const uint32_t boneIdx = it->second;
    if (boneIdx >= m_BoneCount)
    {
        return -1;
    }
    return static_cast<int32_t>(boneIdx);
}

DirectX::XMMATRIX AnimInstance::SampleNodeLocalMatrix(const AnimSection& sec, int32_t nodeIdx, float tSec) const
{
    using namespace DirectX;

    if (!m_Skeleton || nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= m_Skeleton->Nodes.size())
    {
        return XMMatrixIdentity();
    }

    const int32_t boneIdx = FindBoneIndexFromNodeIndex(nodeIdx);
    if (boneIdx >= 0)
    {
        return SampleBoneTransform(sec, static_cast<uint32_t>(boneIdx), tSec).ToMatrix();
    }

    return XMLoadFloat4x4(&m_Skeleton->Nodes[static_cast<size_t>(nodeIdx)].RefLocal);
}

DirectX::XMMATRIX AnimInstance::SampleNodeGlobalMatrix(const AnimSection& sec, int32_t nodeIdx, float tSec) const
{
    using namespace DirectX;

    if (!m_Skeleton || nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= m_Skeleton->Nodes.size())
    {
        return XMMatrixIdentity();
    }

    XMMATRIX global = SampleNodeLocalMatrix(sec, nodeIdx, tSec);
    int32_t parent = m_Skeleton->Nodes[static_cast<size_t>(nodeIdx)].Parent;
    while (parent >= 0 && static_cast<size_t>(parent) < m_Skeleton->Nodes.size())
    {
        global = global * SampleNodeLocalMatrix(sec, parent, tSec);
        parent = m_Skeleton->Nodes[static_cast<size_t>(parent)].Parent;
    }

    return global;
}

Transform AnimInstance::SampleRootMotionBasisTransform(const AnimSection& sec, float tSec) const
{
    if (!m_Skeleton || m_BoneCount == 0 || m_RootBoneIndex < 0 || static_cast<uint32_t>(m_RootBoneIndex) >= m_BoneCount)
    {
        return Transform();
    }

    if (static_cast<size_t>(m_RootBoneIndex) < m_Skeleton->BoneToNode.size())
    {
        const int32_t rootNode = m_Skeleton->BoneToNode[static_cast<size_t>(m_RootBoneIndex)];
        if (rootNode >= 0 && static_cast<size_t>(rootNode) < m_Skeleton->Nodes.size())
        {
            return TransformFromMatrixSafe(SampleNodeGlobalMatrix(sec, rootNode, tSec));
        }
    }

    return SampleBoneTransform(sec, static_cast<uint32_t>(m_RootBoneIndex), tSec);
}

Transform AnimInstance::SampleRootMotionLockedBasisTransform(const AnimSection& sec, float tSec) const
{
    using namespace DirectX;

    if (!m_Skeleton || m_BoneCount == 0 || m_RootBoneIndex < 0 || static_cast<uint32_t>(m_RootBoneIndex) >= m_BoneCount)
    {
        return Transform();
    }

    // Root local(locked) 계산
    XMMATRIX rootLocal = XMMatrixIdentity();
    const uint32_t rootBone = static_cast<uint32_t>(m_RootBoneIndex);
    switch (sec.RootLockMode)
    {
    case ERootLockMode::RefPose:
        if (rootBone < m_Skeleton->RefLocalPose.size())
        {
            rootLocal = XMLoadFloat4x4(&m_Skeleton->RefLocalPose[rootBone]);
        }
        break;

    case ERootLockMode::AnimFirstFrame:
    {
        const Transform firstRoot = m_bHasFirstRootInSection
            ? m_FirstRootInSection
            : SampleBoneTransform(sec, rootBone, 0.f);
        rootLocal = firstRoot.ToMatrix();
        break;
    }

    case ERootLockMode::Zero:
    default:
        rootLocal = XMMatrixIdentity();
        break;
    }

    // Root node parent 글로벌을 붙여, unlocked와 동일한 "basis space"로 맞춘다.
    XMMATRIX parentGlobal = XMMatrixIdentity();
    if (static_cast<size_t>(m_RootBoneIndex) < m_Skeleton->BoneToNode.size())
    {
        const int32_t rootNode = m_Skeleton->BoneToNode[static_cast<size_t>(m_RootBoneIndex)];
        if (rootNode >= 0 && static_cast<size_t>(rootNode) < m_Skeleton->Nodes.size())
        {
            const int32_t parent = m_Skeleton->Nodes[static_cast<size_t>(rootNode)].Parent;
            if (parent >= 0 && static_cast<size_t>(parent) < m_Skeleton->Nodes.size())
            {
                parentGlobal = SampleNodeGlobalMatrix(sec, parent, tSec);
            }
        }
    }

    return TransformFromMatrixSafe(rootLocal * parentGlobal);
}

Transform AnimInstance::ExtractRootDeltaRange(const AnimSection& sec, float tA, float tB) const
{
    using namespace DirectX;

    const Transform a = SampleRootMotionBasisTransform(sec, tA);
    const Transform b = SampleRootMotionBasisTransform(sec, tB);

    const XMMATRIX A = a.ToMatrix();
    const XMMATRIX B = b.ToMatrix();

    XMVECTOR det;
    const XMMATRIX invA = XMMatrixInverse(&det, A);
    const XMMATRIX D = invA * B;

    Transform out = TransformFromRigidMatrix(D);

    // 루트모션 scale 무시 정책
    out.SetScale(1.f, 1.f, 1.f);
    return out;
}

Transform AnimInstance::ExtractRootDeltaLoopAware(const AnimSection& sec, float t0, float t1, bool bWrapped) const
{
    if (!bWrapped || sec.LengthSec <= 1e-6f)
    {
        return ExtractRootDeltaRange(sec, t0, t1);
    }

    // Wrapped range: (t0 -> End) + (End -> 0) + (0 -> t1)
    // End->0 점프를 항상 포함해야 루프 경계 오차가 누적되지 않는다.
    const Transform dA = ExtractRootDeltaRange(sec, t0, sec.LengthSec);
    Transform dWrap = ExtractRootDeltaRange(sec, sec.LengthSec, 0.f);
    // 루프 경계의 미세한 수치 잔차는 장시간 누적으로 drift를 만든다.
    // 거의 identity인 경우에만 스냅해 오차 누적을 차단한다.
    if (IsNearlyIdentityDelta(dWrap, 0.02f, 1.0f))
    {
        dWrap = Transform();
    }
    const Transform dB = ExtractRootDeltaRange(sec, 0.f, t1);
    const DirectX::XMMATRIX M = dA.ToMatrix() * dWrap.ToMatrix() * dB.ToMatrix();

    Transform out = TransformFromRigidMatrix(M);
    out.SetScale(1.f, 1.f, 1.f);
    return out;
}

void AnimInstance::ApplyRootLockToLocalPose(const AnimSection& sec)
{
    using namespace DirectX;

    if (!m_Skeleton) return;
    if (m_RootBoneIndex < 0 || (uint32_t)m_RootBoneIndex >= m_BoneCount) return;
    if (m_LocalPose.size() != m_BoneCount) return;

    const uint32_t root = (uint32_t)m_RootBoneIndex;

    if (sec.bForceRootLock == false && sec.bEnableRootMotion == false)
        return;

    switch (sec.RootLockMode)
    {
    case ERootLockMode::RefPose:
        if (root < m_Skeleton->RefLocalPose.size())
            m_LocalPose[root] = m_Skeleton->RefLocalPose[root];
        break;

    case ERootLockMode::AnimFirstFrame:
        if (!m_bHasFirstRootInSection)
        {
            m_FirstRootInSection = SampleBoneTransform(sec, root, 0.f);
            m_bHasFirstRootInSection = true;
        }
        XMStoreFloat4x4(&m_LocalPose[root], m_FirstRootInSection.ToMatrix());
        break;

    case ERootLockMode::Zero:
    default:
        XMStoreFloat4x4(&m_LocalPose[root], XMMatrixIdentity());
        break;
    }
}

void AnimInstance::SetSequence(const std::shared_ptr<AnimSequenceAsset> seq)
{
    m_Seq = seq;
    m_SectionIndex = 0;
    m_TimeInSection = 0.f;
    m_bPlaying = false;
    
    m_bHasRootDelta = false;
    m_bSkipRootMotionThisTick = true;
    m_bHasFirstRootInSection = false;
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
    
    m_bHasRootDelta = false;
    m_bSkipRootMotionThisTick = true;
    m_bHasFirstRootInSection = false;
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
    
    m_bHasRootDelta = false;
    m_bSkipRootMotionThisTick = false;
}
