#include "CharacterMovementComponenet.h"

#include <algorithm>
#include <cmath>

#include "../../Editor/ComponentReflection.h"
#include "../Animation/AnimInstance.h"
#include "../Characters/Character.h"
#include "../Skeletal/SkeletalMeshComponent.h"
#include "../Skeletal/SkeletalMeshData.h"

using namespace DirectX;

static Transform TransformFromMatrixSafe_Movement(FXMMATRIX M)
{
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

    XMFLOAT3 p, s;
    XMFLOAT4 q;
    XMStoreFloat3(&p, T);
    XMStoreFloat3(&s, S);
    XMStoreFloat4(&q, XMQuaternionNormalize(R));

    out.SetPosition(p.x, p.y, p.z);
    out.SetScale(s.x, s.y, s.z);
    out.SetRotationQuat(q);
    return out;
}

static Transform ComposePostNoScale(const Transform& A, const Transform& D)
{
    Transform out;
    const XMVECTOR qA = XMQuaternionNormalize(A.GetRotationQuat());
    const XMVECTOR qD = XMQuaternionNormalize(D.GetRotationQuat());
    const XMVECTOR qOut = XMQuaternionNormalize(XMQuaternionMultiply(qA, qD));

    const XMVECTOR pA = XMLoadFloat3(&A.m_Pos);
    const XMVECTOR pD = XMLoadFloat3(&D.m_Pos);
    const XMVECTOR pOut = XMVectorAdd(XMVector3Rotate(pA, qD), pD);

    XMFLOAT3 pos{};
    XMFLOAT4 rot{};
    XMStoreFloat3(&pos, pOut);
    XMStoreFloat4(&rot, qOut);
    out.SetPosition(pos.x, pos.y, pos.z);
    out.SetRotationQuat(rot);
    out.SetScale(1.f, 1.f, 1.f);
    return out;
}

static Transform ComposePostNoScale_WithBase(const Transform& Accum, const Transform& Base)
{
    Transform out;
    const XMVECTOR qA = XMQuaternionNormalize(Accum.GetRotationQuat());
    const XMVECTOR qB = XMQuaternionNormalize(Base.GetRotationQuat());
    const XMVECTOR qOut = XMQuaternionNormalize(XMQuaternionMultiply(qA, qB));

    const XMVECTOR pA = XMLoadFloat3(&Accum.m_Pos);
    const XMVECTOR pOut = XMVectorAdd(XMVector3Rotate(pA, qB), XMLoadFloat3(&Base.m_Pos));

    XMFLOAT3 pos{};
    XMFLOAT4 rot{};
    XMStoreFloat3(&pos, pOut);
    XMStoreFloat4(&rot, qOut);
    out.SetPosition(pos.x, pos.y, pos.z);
    out.SetRotationQuat(rot);
    out.SetScale(Base.m_Scale.x, Base.m_Scale.y, Base.m_Scale.z);
    return out;
}

static Transform ConvertMeshSpaceDeltaToRootSpace(
    const Transform& meshSpaceDelta,
    const SkeletalMeshComponent* sk)
{
    using namespace DirectX;

    if (!sk)
    {
        return meshSpaceDelta;
    }

    // Anim root delta는 skeleton node-global 기준이므로,
    // 렌더 파이프라인의 GlobalInverse까지 포함해 root 공간으로 변환한다.
    // Dr = inverse(Mmesh) * inverse(GlobalInverse) * Dmesh * GlobalInverse * Mmesh
    const XMMATRIX MMesh = sk->GetRelativeTransform().ToMatrix();
    const Skeleton* skeleton = sk->GetSkeleton();
    const XMMATRIX MGlobalInv =
        (skeleton != nullptr)
        ? XMLoadFloat4x4(&skeleton->GlobalInverse)
        : XMMatrixIdentity();

    XMVECTOR det{};
    const XMMATRIX MMeshInv = XMMatrixInverse(&det, MMesh);
    if (std::fabs(XMVectorGetX(det)) < 1e-8f)
    {
        return meshSpaceDelta;
    }

    XMVECTOR detGI{};
    const XMMATRIX MGlobalInvInv = XMMatrixInverse(&detGI, MGlobalInv);
    if (std::fabs(XMVectorGetX(detGI)) < 1e-8f)
    {
        return meshSpaceDelta;
    }

    const XMMATRIX DMesh = meshSpaceDelta.ToMatrix();
    const XMMATRIX DRoot = MMeshInv * MGlobalInvInv * DMesh * MGlobalInv * MMesh;

    // DRoot는 강체변환(무스케일)이어야 하므로 MatrixDecompose 대신
    // 평행이동/회전을 직접 추출해 누적 수치오차를 줄인다.
    XMFLOAT4X4 m{};
    XMStoreFloat4x4(&m, DRoot);

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

static bool IsNearlyUniformScale(const DirectX::XMFLOAT3& s, float eps = 1e-4f)
{
    const float ax = std::fabs(s.x);
    const float ay = std::fabs(s.y);
    const float az = std::fabs(s.z);
    return (std::fabs(ax - ay) <= eps) && (std::fabs(ay - az) <= eps);
}

static bool IsNearlySameTransform(
    const Transform& a,
    const Transform& b,
    float posTol = 1e-3f,
    float rotTolDeg = 0.2f,
    float scaleTol = 1e-4f)
{
    using namespace DirectX;

    const float dx = a.m_Pos.x - b.m_Pos.x;
    const float dy = a.m_Pos.y - b.m_Pos.y;
    const float dz = a.m_Pos.z - b.m_Pos.z;
    const float posErr = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (posErr > posTol)
    {
        return false;
    }

    XMFLOAT4 qa{}, qb{};
    XMStoreFloat4(&qa, XMQuaternionNormalize(a.GetRotationQuat()));
    XMStoreFloat4(&qb, XMQuaternionNormalize(b.GetRotationQuat()));
    float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
    dot = std::fabs(dot);
    dot = std::max(-1.0f, std::min(1.0f, dot));
    const float angErrDeg = XMConvertToDegrees(2.0f * std::acos(dot));
    if (angErrDeg > rotTolDeg)
    {
        return false;
    }

    const float sx = std::fabs(a.m_Scale.x - b.m_Scale.x);
    const float sy = std::fabs(a.m_Scale.y - b.m_Scale.y);
    const float sz = std::fabs(a.m_Scale.z - b.m_Scale.z);
    return sx <= scaleTol && sy <= scaleTol && sz <= scaleTol;
}

static Transform ScaleRootMotionTranslationByRootScale(
    const Transform& rootSpaceDelta,
    const SceneComponent* root)
{
    Transform out = rootSpaceDelta;
    if (!root)
    {
        return out;
    }

    const Transform& rootRel = root->GetRelativeTransform();
    const float sx = std::fabs(rootRel.m_Scale.x);
    const float sy = std::fabs(rootRel.m_Scale.y);
    const float sz = std::fabs(rootRel.m_Scale.z);

    out.SetPosition(
        rootSpaceDelta.m_Pos.x * sx,
        rootSpaceDelta.m_Pos.y * sy,
        rootSpaceDelta.m_Pos.z * sz);
    return out;
}

CharacterMovementComponent::CharacterMovementComponent()
{
    TypeName = std::string("CharacterMovementComponent");
    m_RootMotionBase = Transform();
    m_RootMotionAccum = Transform();
}

void CharacterMovementComponent::Tick(float deltaTime)
{
    ActorComponent::Tick(deltaTime);
    ConsumeAndApplyRootMotion(deltaTime);
}

void CharacterMovementComponent::ForceResetRootMotionState()
{
    ResetRootMotionAccumulator();
}

bool CharacterMovementComponent::ShouldApplyRootMotion(const AnimInstance* Anim) const
{
    if (!Anim)
    {
        return false;
    }
    if (!Anim->IsCurrentSectionRootMotionEnabled())
    {
        return false;
    }

    switch (m_RootMotionMode)
    {
    case ERootMotionMode::NoRootMotionExtraction:
    case ERootMotionMode::IgnoreRootMotion:
        return false;

    case ERootMotionMode::RootMotionFromMontagesOnly:
        return Anim->IsCurrentSectionFromMontage();

    case ERootMotionMode::RootMotionFromEverything:
    default:
        return true;
    }
}

void CharacterMovementComponent::ConsumeAndApplyRootMotion(float dt)
{
    (void)dt;

    Character* ownerCharacter = dynamic_cast<Character*>(GetOwner());
    if (!ownerCharacter)
    {
        return;
    }

    SkeletalMeshComponent* sk = ownerCharacter->GetSkeletalComponent();
    if (!sk)
    {
        return;
    }

    AnimInstance* anim = sk->GetAnimInstance();
    if (!anim)
    {
        return;
    }

    SceneComponent* root = ownerCharacter->GetRootComponent();
    if (!root)
    {
        return;
    }

    // 모드 의미를 분리한다:
    // - NoRootMotionExtraction: 추출 자체를 비활성화
    // - IgnoreRootMotion: 추출은 유지하되 이동 적용만 무시
    const bool bEnableExtraction = (m_RootMotionMode != ERootMotionMode::NoRootMotionExtraction);
    if (anim->IsRootMotionExtractionEnabled() != bEnableExtraction)
    {
        anim->SetRootMotionExtractionEnabled(bEnableExtraction);
    }
    if (!bEnableExtraction)
    {
        if (anim->HasExtractedRootMotion())
        {
            (void)anim->ConsumeExtractedRootMotion();
        }
        ResetRootMotionAccumulator();
        return;
    }

    // 비균일 스케일에서는 RootMotion 적용을 금지한다.
    const bool rootUniform = IsNearlyUniformScale(root->GetRelativeTransform().m_Scale);
    const bool meshUniform = IsNearlyUniformScale(sk->GetRelativeTransform().m_Scale);
    const bool bAllowRootMotionByScale = rootUniform && meshUniform;

    if (!bAllowRootMotionByScale)
    {
        if (anim->HasExtractedRootMotion())
        {
            (void)anim->ConsumeExtractedRootMotion();
        }
        ResetRootMotionAccumulator();
        return;
    }

    if (!ShouldApplyRootMotion(anim))
    {
        if (anim->HasExtractedRootMotion())
        {
            (void)anim->ConsumeExtractedRootMotion();
        }
        ResetRootMotionAccumulator();
        return;
    }

    if (!anim->HasExtractedRootMotion())
    {
        ResetRootMotionAccumulator();
        return;
    }

    const Transform deltaMeshSpace = anim->ConsumeExtractedRootMotion();
    const Transform deltaRootSpace = ConvertMeshSpaceDeltaToRootSpace(deltaMeshSpace, sk);
    const Transform deltaScaled = ScaleRootMotionTranslationByRootScale(deltaRootSpace, root);
    MoveWithCollision(deltaScaled);
}

void CharacterMovementComponent::ResetRootMotionAccumulator()
{
    m_bRootMotionAccumInitialized = false;
    m_RootMotionBase = Transform();
    m_RootMotionAccum = Transform();
}

bool CharacterMovementComponent::MoveWithCollision(const Transform& Delta)
{
    // 현재는 충돌 시스템 연결 전: raw 적용
    // TODO: 추후 sweep/slide 구현 시 이 함수 내부만 교체
    ApplyRootMotionDeltaRaw(Delta);
    return true;
}

void CharacterMovementComponent::ApplyRootMotionDeltaRaw(const Transform& Delta)
{
    Character* ownerCharacter = dynamic_cast<Character*>(GetOwner());
    if (!ownerCharacter) return;

    SceneComponent* root = ownerCharacter->GetRootComponent();
    if (!root) return;
    
    Transform& cur = root->GetRelativeTransform();
    // 초기 Actor transform(베이스)과 RootMotion 누적값을 분리해 유지한다.
    // 이렇게 하면 초기 위치/회전 오프셋이 delta 회전에 끌려다니는 문제를 막을 수 있다.
    if (!m_bRootMotionAccumInitialized)
    {
        m_bRootMotionAccumInitialized = true;
        m_RootMotionBase = cur;
        m_RootMotionAccum = Transform();
    }
    else
    {
        const Transform expected = ComposePostNoScale_WithBase(m_RootMotionAccum, m_RootMotionBase);
        // 루트가 외부 시스템(충돌/텔레포트/보정)에 의해 수정되었으면 기준을 재설정한다.
        if (!IsNearlySameTransform(cur, expected))
        {
            m_RootMotionBase = cur;
            m_RootMotionAccum = Transform();
        }
    }

    m_RootMotionAccum = ComposePostNoScale(m_RootMotionAccum, Delta);
    const Transform out = ComposePostNoScale_WithBase(m_RootMotionAccum, m_RootMotionBase);
    cur.SetPosition(out.m_Pos.x, out.m_Pos.y, out.m_Pos.z);
    cur.SetRotationQuat(out.m_RotQ);
    cur.SetScale(out.m_Scale.x, out.m_Scale.y, out.m_Scale.z);
}
