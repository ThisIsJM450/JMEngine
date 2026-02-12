// FBXImporter_Animation.cpp (or FBXImporter_Animation.h/.cpp)
// Drop-in replacement: "boneToChannel (1:1)" 방식 제거하고
// AssimpFbx suffix 채널(_$AssimpFbx$_Rotation/Translation/Scaling)을 본 단위로 merge해서 bake.
// - Node 트리/노드 runtime 필요 없음 (본 only)
// - 즉시 적용: ImportAnimSequence_SectionKeyTransform() 내부의 boneToChannel 구축 + Bake 호출부 교체
// - 아래 코드엔 기존 네 helper (SampleVec3_Ticks, SampleQuat_Ticks, Transform, AnimSection/AnimSequenceAsset 등) 사용

#include "FBXImporter_Animation.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <ostream>
#include <assimp/anim.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "../../Game/Animation/AnimSequence.h"
#include "../../Game/Skeletal/SkeletalMeshData.h"

static int FindKeyIndex_Ticks_Pos(const aiVectorKey* keys, int count, float tTick)
{
    if (!keys || count <= 0) return -1;
    // 마지막 key <= t
    int lo = 0, hi = count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if ((float)keys[mid].mTime <= tTick) lo = mid + 1;
        else hi = mid - 1;
    }
    if (hi < 0) return 0;
    if (hi >= count) return count - 1;
    return hi;
}

static int FindKeyIndex_Ticks_Rot(const aiQuatKey* keys, int count, float tTick)
{
    if (!keys || count <= 0) return -1;
    int lo = 0, hi = count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if ((float)keys[mid].mTime <= tTick) lo = mid + 1;
        else hi = mid - 1;
    }
    if (hi < 0) return 0;
    if (hi >= count) return count - 1;
    return hi;
}

static DirectX::XMFLOAT3 SampleVec3_Ticks(
    const aiVectorKey* keys, int count,
    float tTick,
    const DirectX::XMFLOAT3& defaultV)
{
    if (!keys || count <= 0) return defaultV;
    if (count == 1) return { keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z };

    int i0 = FindKeyIndex_Ticks_Pos(keys, count, tTick);
    int i1 = (i0 + 1 < count) ? (i0 + 1) : i0;

    const float t0 = (float)keys[i0].mTime;
    const float t1 = (float)keys[i1].mTime;

    const aiVector3D& v0 = keys[i0].mValue;
    const aiVector3D& v1 = keys[i1].mValue;

    float s = 0.f;
    const float dt = (t1 - t0);
    if (dt > 1e-6f) s = (tTick - t0) / dt;
    if (s < 0.f) s = 0.f;
    if (s > 1.f) s = 1.f;

    return {
        v0.x + (v1.x - v0.x) * s,
        v0.y + (v1.y - v0.y) * s,
        v0.z + (v1.z - v0.z) * s
    };
}

static Transform TransformFromMatrix(const DirectX::XMFLOAT4X4& m)
{
    using namespace DirectX;

    Transform out;

    XMMATRIX M = XMLoadFloat4x4(&m);

    XMVECTOR S, R, T;
    if (!XMMatrixDecompose(&S, &R, &T, M))
    {
        out.SetPosition(0, 0, 0);
        out.SetScale(1, 1, 1);
        out.SetRotationQuat(XMQuaternionIdentity());
        return out;
    }

    // --- 1) Translation ---
    XMFLOAT3 t3;
    XMStoreFloat3(&t3, T);
    out.SetPosition(t3.x, t3.y, t3.z);

    // --- 2) Scale (clamp tiny) ---
    XMFLOAT3 s3;
    XMStoreFloat3(&s3, S);

    // Decompose가 아주 작은 값을 뱉는 경우를 막음(division/NaN 방지)
    auto ClampTiny = [](float v)
    {
        const float eps = 1e-8f;
        if (fabsf(v) < eps) return (v < 0.f) ? -eps : eps;
        return v;
    };
    s3.x = ClampTiny(s3.x);
    s3.y = ClampTiny(s3.y);
    s3.z = ClampTiny(s3.z);

    // --- 3) Rotation normalize + reflection(negative scale) 처리 ---
    // XMMatrixDecompose는 R이 normalize되어있다고 보장하지 않는 케이스가 있으니 normalize.
    R = XMQuaternionNormalize(R);

    // 반사(det<0) 처리:
    // det(M)이 음수면 미러링이 섞여있다는 뜻. 이 경우 scale 중 하나에 부호를 몰아넣는 식으로 정리해준다.
    // (어느 축에 부호를 몰아넣을지는 정책인데, 보통 X에 몰아넣는 방식이 흔함. 필요하면 가장 큰 축/가장 작은 축으로 바꿔도 됨)
    const float det = XMVectorGetX(XMMatrixDeterminant(M));
    if (det < 0.0f)
    {
        // X축에 반사 부호를 몰아넣기(정책)
        s3.x = -s3.x;

        // 회전도 뒤집어줘야 일관성이 맞는 경우가 많음.
        // 가장 간단한 방식: 180도 회전을 한 축에 적용(여기서는 X축 반전 행렬과 동치인 회전 보정)
        // R = R * qFlip 와 같은 형태로 보정할 수 있음.
        // 다만 이 부분은 프로젝트 컨벤션에 따라 달라서, 우선 안전하게 "축 반전 행렬"을 이용해 재정규화한다.
        //
        // 더 안정적인 방법: M에서 T를 제거하고, scale을 반영한 뒤 회전행렬을 다시 뽑아 quaternion으로 변환.
        // 아래는 그 방식.

        // T 제거
        XMMATRIX MNoT = M;
        MNoT.r[3] = XMVectorSet(0, 0, 0, 1);

        // Scale 제거(절대값 기준)
        const float ax = fabsf(s3.x);
        const float ay = fabsf(s3.y);
        const float az = fabsf(s3.z);

        XMVECTOR invScale = XMVectorSet(
            (ax > 1e-8f) ? (1.0f / ax) : 1.0f,
            (ay > 1e-8f) ? (1.0f / ay) : 1.0f,
            (az > 1e-8f) ? (1.0f / az) : 1.0f,
            1.0f
        );

        // 회전+스케일 파트에서 “스케일 제거” (행/열 기준은 DirectXMath의 행렬 연산 컨벤션에 따름)
        // DirectXMath의 XMMATRIX는 row-major 저장이지만 수학적 의미는 사용 방식에 따라 혼재될 수 있음.
        // 여기선 XMMatrixDecompose가 뽑아준 S를 신뢰하는 대신, 스케일 제거를 대략적으로 수행한 뒤
        // 회전부를 정규직교화한다.
        XMMATRIX Rm = MNoT;

        // 각 basis 벡터를 스케일로 나눠 정규화(열벡터처럼 쓰는 케이스에 흔함)
        // 만약 네 엔진이 행벡터 컨벤션이면 row 기준으로 바꿔야 함.
        // 일단 가장 흔한 “column basis” 형태로 처리:
        Rm.r[0] = XMVectorMultiply(Rm.r[0], XMVectorSplatX(invScale));
        Rm.r[1] = XMVectorMultiply(Rm.r[1], XMVectorSplatY(invScale));
        Rm.r[2] = XMVectorMultiply(Rm.r[2], XMVectorSplatZ(invScale));

        // 정규직교화(Gram-Schmidt)
        XMVECTOR x = XMVector3Normalize(Rm.r[0]);
        XMVECTOR y = Rm.r[1] - x * XMVector3Dot(x, Rm.r[1]);
        y = XMVector3Normalize(y);
        XMVECTOR z = XMVector3Cross(x, y);

        Rm.r[0] = x;
        Rm.r[1] = y;
        Rm.r[2] = z;
        Rm.r[3] = XMVectorSet(0, 0, 0, 1);

        R = XMQuaternionRotationMatrix(Rm);
        R = XMQuaternionNormalize(R);
    }

    out.SetScale(s3.x, s3.y, s3.z);
    out.SetRotationQuat(R);
    return out;
}


static std::vector<Transform> BuildRefPoseTransforms(const Skeleton& skel)
{
    std::vector<Transform> ref;
    ref.resize(skel.RefLocalPose.size());
    for (size_t i = 0; i < ref.size(); ++i)
        ref[i] = TransformFromMatrix(skel.RefLocalPose[i]);
    return ref;
}

static void BuildNodeNameMap(aiNode* n, std::unordered_map<std::string, aiNode*>& out)
{
    if (!n) return;
    out[n->mName.C_Str()] = n;
    for (unsigned i = 0; i < n->mNumChildren; ++i)
        BuildNodeNameMap(n->mChildren[i], out);
}

static Transform TransformFromAssimpNodeLocal(const aiMatrix4x4& mAssimp)
{
    // Assimp 행렬은 row/column 컨벤션이 DirectX와 다를 수 있어서
    // BestCode처럼 transpose 후 decompose하는 편이 안전(너희 기존 로직과 일치).
    aiMatrix4x4 mT = mAssimp;

    // aiMatrix4x4 -> XMFLOAT4X4로 복사
    DirectX::XMFLOAT4X4 m;
    m._11 = mT.a1; m._12 = mT.a2; m._13 = mT.a3; m._14 = mT.a4;
    m._21 = mT.b1; m._22 = mT.b2; m._23 = mT.b3; m._24 = mT.b4;
    m._31 = mT.c1; m._32 = mT.c2; m._33 = mT.c3; m._34 = mT.c4;
    m._41 = mT.d1; m._42 = mT.d2; m._43 = mT.d3; m._44 = mT.d4;

    return TransformFromMatrix(m); // 네 코드에 이미 있음
}

static DirectX::XMFLOAT4 SampleQuat_Ticks(
    const aiQuatKey* keys, int count,
    float tTick,
    const DirectX::XMFLOAT4& defaultQ)
{
    using namespace DirectX;

    if (!keys || count <= 0) return defaultQ;
    if (count == 1)
    {
        const aiQuaternion& q = keys[0].mValue;
        XMFLOAT4 out{ q.x, q.y, q.z, q.w };
        // normalize
        XMVECTOR Q = XMLoadFloat4(&out);
        XMStoreFloat4(&out, XMQuaternionNormalize(Q));
        return out;
    }

    int i0 = FindKeyIndex_Ticks_Rot(keys, count, tTick);
    int i1 = (i0 + 1 < count) ? (i0 + 1) : i0;

    const float t0 = (float)keys[i0].mTime;
    const float t1 = (float)keys[i1].mTime;

    aiQuaternion qa = keys[i0].mValue;
    aiQuaternion qb = keys[i1].mValue;

    // hemisphere fix: dot < 0이면 qb 부호 반전
    const float dot = qa.x*qb.x + qa.y*qb.y + qa.z*qb.z + qa.w*qb.w;
    if (dot < 0.f)
    {
        qb.x = -qb.x; qb.y = -qb.y; qb.z = -qb.z; qb.w = -qb.w;
    }

    float s = 0.f;
    const float dt = (t1 - t0);
    if (dt > 1e-6f) s = (tTick - t0) / dt;
    s = std::clamp(s, 0.f, 1.f);

    XMFLOAT4 fqa{ qa.x, qa.y, qa.z, qa.w };
    XMFLOAT4 fqb{ qb.x, qb.y, qb.z, qb.w };

    XMVECTOR Q0 = XMLoadFloat4(&fqa);
    XMVECTOR Q1 = XMLoadFloat4(&fqb);

    XMVECTOR Q = XMQuaternionSlerp(XMVector4Normalize(Q0), XMVector4Normalize(Q1), s);
    Q = XMQuaternionNormalize(Q);
    
    XMFLOAT4 out;
    XMStoreFloat4(&out, Q);
    return out;
}
// ======================================================================
// 1) Assimp FBX channel suffix handling
// ======================================================================

enum class EAssimpFbxProp { None, Translation, Rotation, Scaling };

static std::string StripAssimpFbxSuffix(const std::string& in, EAssimpFbxProp& outProp)
{
    outProp = EAssimpFbxProp::None;

    // Example: "mixamorig:Head_$AssimpFbx$_Rotation"
    const std::string tag = "_$AssimpFbx$_";
    const size_t p = in.find(tag);
    if (p == std::string::npos)
        return in;

    std::string base = in.substr(0, p);
    std::string suf  = in.substr(p + tag.size());

    if (suf == "Translation") outProp = EAssimpFbxProp::Translation;
    else if (suf == "Rotation") outProp = EAssimpFbxProp::Rotation;
    else if (suf == "Scaling") outProp = EAssimpFbxProp::Scaling;
    else outProp = EAssimpFbxProp::None;

    return base;
}

// If you already have NormalizeBoneName(), keep it and use it.
// This version merges your prior rules + strips suffix.
static std::string NormalizeName_All(std::string s)
{
    // 1) suffix strip
    EAssimpFbxProp dummy;
    s = StripAssimpFbxSuffix(s, dummy);

    // 2) your existing normalize rules
    const char* pfx = "mixamorig:";
    if (s.rfind(pfx, 0) == 0) s = s.substr(strlen(pfx));

    const size_t bar = s.find('|');
    if (bar != std::string::npos) s = s.substr(bar + 1);

    return s;
}

struct FbxMergedTrack
{
    const aiNodeAnim* Full = nullptr; // suffix 없는 일반 채널이 들어올 수 있음
    const aiNodeAnim* Pos  = nullptr; // _$AssimpFbx$_Translation
    const aiNodeAnim* Rot  = nullptr; // _$AssimpFbx$_Rotation
    const aiNodeAnim* Scl  = nullptr; // _$AssimpFbx$_Scaling
};

static void ApplyChannel_Pos(const aiNodeAnim* ch, float tTick, const Transform& refDefault, Transform& ioTr)
{
    if (!ch || ch->mNumPositionKeys == 0) return;
    DirectX::XMFLOAT3 T = SampleVec3_Ticks(ch->mPositionKeys, (int)ch->mNumPositionKeys, tTick, refDefault.m_Pos);
    ioTr.SetPosition(T.x, T.y, T.z);
}

static void ApplyChannel_Scl(const aiNodeAnim* ch, float tTick, const Transform& refDefault, Transform& ioTr)
{
    if (!ch || ch->mNumScalingKeys == 0) return;
    DirectX::XMFLOAT3 S = SampleVec3_Ticks(ch->mScalingKeys, (int)ch->mNumScalingKeys, tTick, refDefault.m_Scale);
    ioTr.SetScale(S.x, S.y, S.z);
}

static void ApplyChannel_Rot(const aiNodeAnim* ch, float tTick, const Transform& refDefault, Transform& ioTr)
{
    if (!ch || ch->mNumRotationKeys == 0) return;

    DirectX::XMFLOAT4 defQ;

    DirectX::XMFLOAT4 Q = SampleQuat_Ticks(ch->mRotationKeys, (int)ch->mNumRotationKeys, tTick, defQ);
    ioTr.SetRotationQuat(Q);
}

// merged track을 하나의 Transform로 채우는 함수
static void FillTransformFromMergedTrack_Ticks(
    const FbxMergedTrack& tr,
    float tTick,
    const Transform& refDefault,
    Transform& outTr)
{
    // default = ref
    outTr = refDefault;
    
    // suffix 없는 Full 채널이 있으면 우선 적용
    // (FBX에 따라 Full에 pos/rot/scl 모두 있거나 일부만 있을 수 있음)
    if (tr.Full)
    {
        ApplyChannel_Pos(tr.Full, tTick, refDefault, outTr);
        ApplyChannel_Scl(tr.Full, tTick, refDefault, outTr);
        ApplyChannel_Rot(tr.Full, tTick, refDefault, outTr);
    }
    else
    {
        // 분리 채널이 있으면 해당 축만 덮어쓰기
        
        ApplyChannel_Scl(tr.Scl, tTick, refDefault, outTr);
        ApplyChannel_Rot(tr.Rot, tTick, refDefault, outTr);
        
        ApplyChannel_Pos(tr.Pos, tTick, refDefault, outTr);
        ApplyChannel_Rot(tr.Rot, tTick, refDefault, outTr);
        
        ApplyChannel_Pos(tr.Pos, tTick, refDefault, outTr);
        ApplyChannel_Scl(tr.Scl, tTick, refDefault, outTr);
        
        ApplyChannel_Pos(tr.Pos, tTick, refDefault, outTr);
        ApplyChannel_Scl(tr.Scl, tTick, refDefault, outTr);
        ApplyChannel_Rot(tr.Rot, tTick, refDefault, outTr);
    }
}

// ======================================================================
// 2) Bake: boneCount 기준으로 merged track을 사용해 Keys 생성
// ======================================================================

static AnimSection BakeSectionFromAssimp_MergedTracks(
    const aiAnimation* anim,
    const aiScene* scene,
    const Skeleton& skel,
    const std::vector<FbxMergedTrack>& boneTracks, // boneIndex -> merged track
    float startSec,
    float lengthSec,
    float sampleRate,
    uint32_t boneCount)
{
    std::vector<Transform> refPose = BuildRefPoseTransforms(skel);
    if (refPose.size() != boneCount)
        refPose.resize(boneCount);
    
    std::unordered_map<std::string, aiNode*> nodeMap;
    nodeMap.reserve(2048);
    BuildNodeNameMap(scene->mRootNode, nodeMap);
    
    std::vector<Transform> srcNodeDefaults;
    srcNodeDefaults.resize(boneCount);

    for (size_t bi = 0; bi < skel.Bones.size(); ++bi)
    {
        std::string boneName = skel.Bones[bi].Name;

        // 채널 merge에 쓰는 것과 동일한 normalize 적용 권장
        // boneName = NormalizeName_All(boneName);

        auto itN = nodeMap.find(boneName);
        if (itN != nodeMap.end() && itN->second)
            srcNodeDefaults[bi] = TransformFromAssimpNodeLocal(itN->second->mTransformation);
        else
            srcNodeDefaults[bi] = TransformFromMatrix(skel.RefLocalPose[bi]); // fallback
    }

    AnimSection sec;
    sec.Name = "Default";
    sec.StartSec = startSec;
    sec.LengthSec = lengthSec;
    sec.SampleRate = sampleRate;
    //sec.RefPose = srcNodeDefaults;

    sec.NumFrames = (uint32_t)std::floor(lengthSec * sampleRate) + 1;
    sec.Keys.resize((size_t)sec.NumFrames * boneCount);

    const float tps = (anim->mTicksPerSecond > 1e-6) ? (float)anim->mTicksPerSecond : 30.f;

    for (uint32_t f = 0; f < sec.NumFrames; ++f)
    {
        float tSec = (float)f / sampleRate;
        float tTick = (startSec + tSec) * tps;

        for (uint32_t b = 0; b < boneCount; ++b)
        {
            // 프레임마다 누적 X
            Transform tr;
            FillTransformFromMergedTrack_Ticks(boneTracks[b], tTick, f == 0 ? refPose[b] : sec.Keys[((size_t)f-1) * boneCount + b], tr);
            sec.Keys[(size_t)f * boneCount + b] = tr;
        }
    }
    return sec;
}

// ======================================================================
// 3) PUBLIC: ImportAnimSequence_SectionKeyTransform - ONLY importer logic changed
// ======================================================================

static uint64_t MakeSkeletonHash(const Skeleton& skel)
{
    uint64_t h = 1469598103934665603ull; // FNV offset
    for (const auto& b : skel.Bones)
    {
        for (unsigned char c : b.Name)
        {
            h ^= (uint64_t)c;
            h *= 1099511628211ull;
        }
    }
    return h;
}

std::shared_ptr<AnimSequenceAsset>
FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(
    const std::string& fbxPath,
    const Skeleton& targetSkeleton,
    float sampleRate)
{
    Assimp::Importer importer;

    unsigned flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                     aiProcess_ImproveCacheLocality | aiProcess_SortByPType |
                     aiProcess_ConvertToLeftHanded;

    const aiScene* scene = importer.ReadFile(fbxPath, flags);
    if (!scene || !scene->HasAnimations()) return nullptr;

    const aiAnimation* anim = scene->mAnimations[0];
    if (!anim) return nullptr;

    auto seq = std::make_shared<AnimSequenceAsset>();
    seq->Name = anim->mName.length ? anim->mName.C_Str() : "Anim";
    seq->SkeletonHash = MakeSkeletonHash(targetSkeleton);
    seq->BoneCount = (uint32_t)targetSkeleton.Bones.size();

    const float tps = (anim->mTicksPerSecond > 1e-6) ? (float)anim->mTicksPerSecond : 30.f;
    const float lengthSec = (anim->mDuration > 1e-6) ? (float)anim->mDuration / tps : 0.f;
    seq->TotalLengthSec = lengthSec;

    // ------------------------------------------------------------------
    // [CHANGED] 1) channel들을 "베이스 본 이름" 기준으로 merge
    // ------------------------------------------------------------------
    std::unordered_map<std::string, FbxMergedTrack> merged;
    merged.reserve(anim->mNumChannels);

    for (unsigned c = 0; c < anim->mNumChannels; ++c)
    {
        const aiNodeAnim* ch = anim->mChannels[c];
        if (!ch) continue;

        std::string raw = ch->mNodeName.C_Str();

        EAssimpFbxProp prop;
        std::string base = StripAssimpFbxSuffix(raw, prop);

        // Normalize on "base"
        // std::string key = NormalizeName_All(base);
        std::string key = base;

        FbxMergedTrack& tr = merged[key];

        if (prop == EAssimpFbxProp::Translation) tr.Pos = ch;
        else if (prop == EAssimpFbxProp::Rotation) tr.Rot = ch;
        else if (prop == EAssimpFbxProp::Scaling) tr.Scl = ch;
        else tr.Full = ch; // suffix 없는 일반 채널
    }

    // ------------------------------------------------------------------
    // [CHANGED] 2) bone index -> merged track 매핑
    //  - targetSkeleton.BoneNameToIndex는 이미 있다고 가정
    //  - 여기서는 "본 리스트 순서"대로 track을 채운다
    // ------------------------------------------------------------------
    std::vector<FbxMergedTrack> boneTracks;
    boneTracks.resize(targetSkeleton.Bones.size());

    for (size_t bi = 0; bi < targetSkeleton.Bones.size(); ++bi)
    {
        const std::string& boneNameRaw = targetSkeleton.Bones[bi].Name;
        // std::string key = NormalizeName_All(boneNameRaw);
        EAssimpFbxProp Prop;
        std::string key = StripAssimpFbxSuffix(boneNameRaw, Prop);

        auto it = merged.find(key);
        if (it != merged.end())
        {
            boneTracks[bi] = it->second;
        }
        else
        {
            // 매칭 실패 시: ref pose로만 남음 (안전)
            boneTracks[bi] = FbxMergedTrack{};
            std::cout << key<< " is not contain in Animtation" << std::endl;
        }
    }

    // ------------------------------------------------------------------
    // [CHANGED] 3) bake는 merged track 기반
    // ------------------------------------------------------------------
    AnimSection sec = BakeSectionFromAssimp_MergedTracks(
        anim,
        scene,
        targetSkeleton,
        boneTracks,
        /*startSec*/0.f,
        /*lengthSec*/lengthSec,
        /*sampleRate*/sampleRate,
        /*boneCount*/seq->BoneCount);

    sec.Name = "Default";
    seq->Sections.push_back(std::move(sec));
    return seq;
}
