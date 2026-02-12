#include "FbxImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../Dx11Context.h"
#include "../../Game/MeshData.h"  
#include "FbxImportUtils.h"
#include "FbxMaterialBuilder.h"

#include <cmath>
#include <iomanip>
#include <iostream>

#include "../../Game/Skeletal/SkeletalMeshData.h"

using namespace FbxImportUtils;


static void AppendMeshTransformed_PerMaterial(
    const aiScene* scene,
    const aiMesh* mesh,
    const aiMatrix4x4& global,
    MeshAsset& outAsset,
    PerMatIndices& outPerMatIndices,
    std::vector<uint8_t>& outUsedMaterials)
{
    if (!mesh) return;

    const aiMatrix3x3 normalMat = MakeNormalMatrix(global);

    const bool hasNormals = mesh->HasNormals();
    const bool hasUV0 = mesh->HasTextureCoords(0);
    const bool hasUV1 = mesh->HasTextureCoords(1);
    const int  uvCh = hasUV0 ? 0 : (hasUV1 ? 1 : -1);

    const uint32_t baseV = (uint32_t)outAsset.Vertices.size();
    outAsset.Vertices.reserve(outAsset.Vertices.size() + mesh->mNumVertices);

    // PASS1: vertices
    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex v{};

        aiVector3D p = mesh->mVertices[i];
        p = global * p;
        v.Pos = ToXM3(p);

        aiVector3D n = hasNormals ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
        //n = normalMat * n;
        n.Normalize();
        v.Normal = ToXM3(n);

        if (uvCh >= 0)
        {
            const aiVector3D uv = mesh->mTextureCoords[uvCh][i];
            v.UV = ToXM2(uv);
        }
        else
        {
            v.UV = { 0, 0 };
        }

        v.Tangent = { 1, 0, 0, 1 };

        outAsset.Vertices.push_back(v);
    }

    const uint32_t matIndex = mesh->mMaterialIndex;
    if (matIndex < outUsedMaterials.size())
        outUsedMaterials[matIndex] = 1;

    // PASS2: per-material indices
    auto& idxList = outPerMatIndices[matIndex];
    idxList.reserve(idxList.size() + mesh->mNumFaces * 3);

    for (unsigned f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;

        idxList.push_back(baseV + (uint32_t)face.mIndices[0]);
        idxList.push_back(baseV + (uint32_t)face.mIndices[1]);
        idxList.push_back(baseV + (uint32_t)face.mIndices[2]);
    }

    // PASS3: tangent
    if (mesh->HasTangentsAndBitangents())
    {
        aiMatrix3x3 m3(global);
        for (unsigned i = 0; i < mesh->mNumVertices; ++i)
        {
            aiVector3D t = mesh->mTangents[i];
            t = m3 * t;
            t.Normalize();
            outAsset.Vertices[baseV + i].Tangent = { t.x, t.y, t.z, 1.f };
        }
        return;
    }

    if (uvCh < 0)
    {
        for (unsigned i = 0; i < mesh->mNumVertices; ++i)
            outAsset.Vertices[baseV + i].Tangent = { 1,0,0,1 };
        return;
    }

    std::vector<DirectX::XMFLOAT3> tanSum(mesh->mNumVertices, { 0,0,0 });
    std::vector<DirectX::XMFLOAT3> bitanSum(mesh->mNumVertices, { 0,0,0 });

    auto Add3 = [](DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        a.x += b.x; a.y += b.y; a.z += b.z;
    };

    for (unsigned f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;

        const uint32_t i0 = face.mIndices[0];
        const uint32_t i1 = face.mIndices[1];
        const uint32_t i2 = face.mIndices[2];

        const Vertex& v0 = outAsset.Vertices[baseV + i0];
        const Vertex& v1 = outAsset.Vertices[baseV + i1];
        const Vertex& v2 = outAsset.Vertices[baseV + i2];

        const float x1 = v1.Pos.x - v0.Pos.x;
        const float y1 = v1.Pos.y - v0.Pos.y;
        const float z1 = v1.Pos.z - v0.Pos.z;

        const float x2 = v2.Pos.x - v0.Pos.x;
        const float y2 = v2.Pos.y - v0.Pos.y;
        const float z2 = v2.Pos.z - v0.Pos.z;

        const float s1 = v1.UV.x - v0.UV.x;
        const float t1 = v1.UV.y - v0.UV.y;

        const float s2 = v2.UV.x - v0.UV.x;
        const float t2 = v2.UV.y - v0.UV.y;

        const float denom = (s1 * t2 - s2 * t1);
        if (fabs(denom) < 1e-8f) continue;

        const float r = 1.0f / denom;

        DirectX::XMFLOAT3 tangent{
            (t2 * x1 - t1 * x2) * r,
            (t2 * y1 - t1 * y2) * r,
            (t2 * z1 - t1 * z2) * r
        };

        DirectX::XMFLOAT3 bitangent{
            (s1 * x2 - s2 * x1) * r,
            (s1 * y2 - s2 * y1) * r,
            (s1 * z2 - s2 * z1) * r
        };

        Add3(tanSum[i0], tangent); Add3(tanSum[i1], tangent); Add3(tanSum[i2], tangent);
        Add3(bitanSum[i0], bitangent); Add3(bitanSum[i1], bitangent); Add3(bitanSum[i2], bitangent);
    }

    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex& v = outAsset.Vertices[baseV + i];

        using namespace DirectX;
        XMVECTOR N = XMLoadFloat3(&v.Normal);
        XMVECTOR T = XMLoadFloat3(&tanSum[i]);
        XMVECTOR B = XMLoadFloat3(&bitanSum[i]);

        if (XMVector3LessOrEqual(XMVector3LengthSq(T), XMVectorReplicate(1e-12f)))
        {
            v.Tangent = { 1,0,0,1 };
            continue;
        }

        T = XMVector3Normalize(T - N * XMVector3Dot(N, T));
        float handed = (XMVectorGetX(XMVector3Dot(XMVector3Cross(N, T), B)) < 0.0f) ? -1.0f : 1.0f;

        XMFLOAT3 t3;
        XMStoreFloat3(&t3, T);

        v.Tangent = { t3.x, t3.y, t3.z, handed };
    }
}

static void TraverseAndBuild_PerMaterial(
    const aiScene* scene,
    const aiNode* node,
    const aiMatrix4x4& parent,
    MeshAsset& outAsset,
    PerMatIndices& outPerMatIndices,
    std::vector<uint8_t>& outUsedMaterials)
{
    aiMatrix4x4 global = parent * node->mTransformation;

    for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
    {
        const unsigned meshIndex = node->mMeshes[mi];
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) continue;

        AppendMeshTransformed_PerMaterial(scene, mesh, global, outAsset, outPerMatIndices, outUsedMaterials);
    }

    for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
    {
        TraverseAndBuild_PerMaterial(scene, node->mChildren[ci], global, outAsset, outPerMatIndices, outUsedMaterials);
    }
        
}


// ------------------------------------------------------------
// Skeletal: per-material build (global bake 금지)
// ------------------------------------------------------------

static void BuildGlobalNode_Assimp(
    const aiNode* n,
    const aiMatrix4x4& parent,
    std::unordered_map<std::string, aiMatrix4x4>& outGlobal)
{
    aiMatrix4x4 g = parent * n->mTransformation;
    outGlobal[n->mName.C_Str()] = g;

    for (unsigned i = 0; i < n->mNumChildren; ++i)
        BuildGlobalNode_Assimp(n->mChildren[i], g, outGlobal);
}



static void AppendSkinnedMesh_PerMaterial_Rebuild(
    const aiScene* scene,
    const aiMesh* mesh,
    const aiMatrix4x4& meshGlobal, // owner node global
    SkeletalMeshAsset& outAsset,
    PerMatIndices& outPerMatIndices,
    std::vector<uint8_t>& outUsedMaterials)
{
    if (!mesh) return;

    const bool hasNormals = mesh->HasNormals();
    const bool hasUV0 = mesh->HasTextureCoords(0);
    const bool hasUV1 = mesh->HasTextureCoords(1);
    const int  uvCh = hasUV0 ? 0 : (hasUV1 ? 1 : -1);

    const aiMatrix3x3 normalMat = MakeNormalMatrix(meshGlobal);
    const aiMatrix3x3 tangentMat(meshGlobal); // tangent는 3x3만 적용(스케일 포함이면 정규화)

    const uint32_t baseV = (uint32_t)outAsset.Vertices.size();
    outAsset.Vertices.resize(baseV + mesh->mNumVertices);

    // ------------------------------------
    // PASS1: vertices (meshGlobal bake)
    // ------------------------------------
    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        SkinnedVertex v{};

        // pos bake
        aiVector3D p = mesh->mVertices[i];
        //p = meshGlobal * p;
        v.Pos = { p.x, p.y, p.z };

        // normal bake
        aiVector3D n = hasNormals ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
        //n = normalMat * n;
        n.Normalize();
        v.Normal = { n.x, n.y, n.z };

        // uv
        if (uvCh >= 0)
        {
            const aiVector3D uv = mesh->mTextureCoords[uvCh][i];
            v.UV = { uv.x, uv.y };
        }
        else v.UV = { 0, 0 };

        // tangent (있으면 bake)
        if (mesh->HasTangentsAndBitangents())
        {
            aiVector3D t = mesh->mTangents[i];
            //t = tangentMat * t;
            t.Normalize();
            v.Tangent = { t.x, t.y, t.z, 1.f };
        }
        else
        {
            v.Tangent = { 1, 0, 0, 1 };
        }

        // bone data 초기값은 0으로 (SkinnedVertex 기본값이 0이면 생략 가능)
        // v.BoneIndex[], v.BoneWeight[]는 ctor/default로 0 가정

        outAsset.Vertices[baseV + i] = v;
    }

    // ------------------------------------
    // PASS2: bones / weights
    //  - 여기서는 boneIndex는 "스켈레톤 전체에서의 인덱스"로 통일
    // ------------------------------------
    for (unsigned b = 0; b < mesh->mNumBones; ++b)
    {
        const aiBone* bone = mesh->mBones[b];
        if (!bone) continue;

        const std::string boneName = bone->mName.C_Str();

        uint32_t boneIndex = 0;
        auto it = outAsset.Skeleton.BoneNameToIndex.find(boneName);
        if (it == outAsset.Skeleton.BoneNameToIndex.end())
        {
            boneIndex = (uint32_t)outAsset.Skeleton.Bones.size();
            outAsset.Skeleton.BoneNameToIndex[boneName] = boneIndex;

            BoneInfo bi{};
            bi.Name   = boneName;
            bi.OffsetA = bone->mOffsetMatrix;      // 원본
            bi.Offset  = ToXM4(bone->mOffsetMatrix); // 기존 유지
            outAsset.Skeleton.Bones.push_back(bi);
        }
        else
        {
            boneIndex = it->second;
        }

        for (unsigned w = 0; w < bone->mNumWeights; ++w)
        {
            const aiVertexWeight& vw = bone->mWeights[w];
            const uint32_t vi = baseV + (uint32_t)vw.mVertexId;
            if (vi >= outAsset.Vertices.size())
            {
                continue;
            }

            AddBoneData(outAsset.Vertices[vi], boneIndex, vw.mWeight);
        }
    }

    // normalize weights
    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
        NormalizeWeights(outAsset.Vertices[baseV + i]);

    // ------------------------------------
    // PASS3: indices per material
    // ------------------------------------
    const uint32_t matIndex = mesh->mMaterialIndex;
    if (matIndex < outUsedMaterials.size())
        outUsedMaterials[matIndex] = 1;

    auto& idxList = outPerMatIndices[matIndex];
    idxList.reserve(idxList.size() + mesh->mNumFaces * 3);

    for (unsigned f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;

        idxList.push_back(baseV + (uint32_t)face.mIndices[0]);
        idxList.push_back(baseV + (uint32_t)face.mIndices[1]);
        idxList.push_back(baseV + (uint32_t)face.mIndices[2]);
    }
}
static void TraverseAndBuild_Skinned_PerMaterial_Rebuild(
    const aiScene* scene,
    const aiNode* node,
    const aiMatrix4x4& parent,
    SkeletalMeshAsset& outAsset,
    PerMatIndices& outPerMatIndices,
    std::vector<uint8_t>& outUsedMaterials)
{
    aiMatrix4x4 global = parent * node->mTransformation;

    for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
    {
        const unsigned meshIndex = node->mMeshes[mi];
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) continue;

        // meshGlobal = 이 node의 global
        AppendSkinnedMesh_PerMaterial_Rebuild(scene, mesh, global, outAsset, outPerMatIndices, outUsedMaterials);
    }

    for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        TraverseAndBuild_Skinned_PerMaterial_Rebuild(scene, node->mChildren[ci], global, outAsset, outPerMatIndices, outUsedMaterials);
}

static void BuildBindPoseBonePalette_Rebuild(
    const aiScene* scene,
    SkeletalMeshAsset& sk)
{
    // 1) global node map (assimp)
    std::unordered_map<std::string, aiMatrix4x4> globalNodeA;
    globalNodeA.reserve(2048);
    BuildGlobalNode_Assimp(scene->mRootNode, aiMatrix4x4(), globalNodeA);

    // 2) invRoot (assimp inverse)
    aiMatrix4x4 invRootA = scene->mRootNode->mTransformation;
    invRootA.Inverse();

    // 3) palette
    sk.BonePalette.resize(sk.Skeleton.Bones.size());

    for (size_t i = 0; i < sk.Skeleton.Bones.size(); ++i)
    {
        const BoneInfo& bi = sk.Skeleton.Bones[i];

        auto it = globalNodeA.find(bi.Name);
        if (it == globalNodeA.end())
        {
            continue;
        }

        const aiMatrix4x4& nodeGA = it->second;
        const aiMatrix4x4& offsetA = bi.OffsetA;

        aiMatrix4x4 finalA = invRootA * nodeGA * offsetA;

        // 마지막에만 변환
        sk.BonePalette[i] = ToXM4(finalA);
    }
}

static const aiNode* FindNodeByNameRecursive(const aiNode* node, const std::string& name)
{
    if (!node) return nullptr;
    if (name == node->mName.C_Str()) return node;

    for (unsigned i = 0; i < node->mNumChildren; ++i)
    {
        if (const aiNode* found = FindNodeByNameRecursive(node->mChildren[i], name))
            return found;
    }
    return nullptr;
}

static std::string NormalizeBoneName(std::string s)
{
    const char* pfx = "mixamorig:";
    if (s.rfind(pfx, 0) == 0) s = s.substr(strlen(pfx));
    auto bar = s.find('|');
    if (bar != std::string::npos) s = s.substr(bar + 1);
    return s;
}

static void BuildRefLocalPoseFromScene(
    const aiScene* scene,
    Skeleton& skel,
    bool bUseNormalizeName = false)
{
    using namespace DirectX;

    const size_t boneCount = skel.Bones.size();
    skel.RefLocalPose.resize(boneCount);

    // 1) 기본은 Identity
    for (size_t i = 0; i < boneCount; ++i)
        XMStoreFloat4x4(&skel.RefLocalPose[i], XMMatrixIdentity());

    if (!scene || !scene->mRootNode) return;

    // 2) 각 BoneInfo.Name에 대응하는 aiNode의 local transform을 넣는다
    for (size_t i = 0; i < boneCount; ++i)
    {
        std::string want = skel.Bones[i].Name;
        if (bUseNormalizeName)
            want = NormalizeBoneName(want);

        // 노드 이름도 normalize해서 비교하려면 전체 트리를 순회하며 비교해야 해서
        // 여기서는 "스켈레톤이 저장한 이름과 노드 이름이 동일"하다는 가정으로 Find.
        // (Mixamo prefix 문제가 있으면 아래 주석 처리된 방식으로 바꾸는게 낫다)
        const aiNode* node = FindNodeByNameRecursive(scene->mRootNode, want);

        if (!node)
        {
            continue;
        }

        XMFLOAT4X4 localF = ToXM4(node->mTransformation); 
        skel.RefLocalPose[i] = localF;
    }
}

static int32_t BuildNodeTree_Assimp_Recursive(
    const aiNode* n,
    int32_t parentIdx,
    Skeleton& skel)
{
    if (!n) return -1;

    // (A) 노드 하나 추가
    const int32_t myIdx = (int32_t)skel.Nodes.size();
    skel.Nodes.emplace_back();

    NodeInfo& node = skel.Nodes.back();
    node.Name = n->mName.C_Str();
    node.Parent = parentIdx;

    // LOCAL 저장 (글로벌 계산 X)
    {
        const aiMatrix4x4& m = n->mTransformation;
        node.RefLocal._11 = m.a1; node.RefLocal._12 = m.a2; node.RefLocal._13 = m.a3; node.RefLocal._14 = m.a4;
        node.RefLocal._21 = m.b1; node.RefLocal._22 = m.b2; node.RefLocal._23 = m.b3; node.RefLocal._24 = m.b4;
        node.RefLocal._31 = m.c1; node.RefLocal._32 = m.c2; node.RefLocal._33 = m.c3; node.RefLocal._34 = m.c4;
        node.RefLocal._41 = m.d1; node.RefLocal._42 = m.d2; node.RefLocal._43 = m.d3; node.RefLocal._44 = m.d4;
    }

    node.Children.clear();
    node.Children.reserve(n->mNumChildren);

    // (B) 이름->인덱스 등록
    skel.NodeNameToIndex[node.Name] = myIdx;

    // (C) 자식 재귀
    for (uint32_t i = 0; i < n->mNumChildren; ++i)
    {
        const int32_t childIdx = BuildNodeTree_Assimp_Recursive(n->mChildren[i], myIdx, skel);
        if (childIdx >= 0)
            skel.Nodes[myIdx].Children.push_back(childIdx);
    }

    return myIdx;
}

static void BuildNodeTreeAndBoneMapping_Assimp(
    const aiScene* scene,
    Skeleton& skel)
{
    if (!scene || !scene->mRootNode)
        return;

    // 노드 정보 싹 비우고 다시 채움
    skel.Nodes.clear();
    skel.NodeNameToIndex.clear();
    skel.BoneToNode.clear();

    skel.Nodes.reserve(2048);
    skel.NodeNameToIndex.reserve(2048);

    // ✅ 노드 트리 정보 채우기 (LOCAL만 저장)
    BuildNodeTree_Assimp_Recursive(scene->mRootNode, -1, skel);

    // ✅ bone -> node 매핑 (bone name == node name 가정)
    skel.BoneToNode.assign(skel.Bones.size(), -1);

    for (int32_t bi = 0; bi < (int32_t)skel.Bones.size(); ++bi)
    {
        const std::string& boneName = skel.Bones[bi].Name; // BoneInfo에 Name 있어야 함
        auto it = skel.NodeNameToIndex.find(boneName);
        if (it != skel.NodeNameToIndex.end())
            skel.BoneToNode[bi] = it->second;
    }

    // (선택) 바로 눈으로 확인할 수 있게 최소 로그
    std::cout << "[NodeBuild] Nodes=" << skel.Nodes.size()
              << " NodeMap=" << skel.NodeNameToIndex.size()
              << " Bones=" << skel.Bones.size()
              << " BoneToNode=" << skel.BoneToNode.size() << "\n";
}


// ------------------------------------------------------------
// FbxImporter methods
// ------------------------------------------------------------

bool FbxImporter::BuildStatic(
    const aiScene* scene,
    const ImportOptions& opt,
    std::shared_ptr<MeshAsset>& outMesh,
    std::vector<uint8_t>& outUsedMaterials)
{
    (void)opt;

    auto meshAsset = std::make_shared<MeshAsset>();
    meshAsset->Stride = sizeof(Vertex);

    PerMatIndices perMat;
    outUsedMaterials.assign(scene->mNumMaterials, 0);

    TraverseAndBuild_PerMaterial(scene, scene->mRootNode, aiMatrix4x4(), *meshAsset, perMat, outUsedMaterials);
    FbxImportUtils::FinalizeIndexBufferAndSections(perMat, outUsedMaterials, *meshAsset);

    if (meshAsset->Vertices.empty() || meshAsset->Indices.empty() || meshAsset->Sections.empty())
        return false;

    outMesh = std::move(meshAsset);
    return true;
}


bool FbxImporter::BuildSkeletal(
    const aiScene* scene,
    const ImportOptions& opt,
    std::shared_ptr<SkeletalMeshAsset>& outMesh,
    std::vector<uint8_t>& outUsedMaterials)
{
    (void)opt;

    auto sk = std::make_shared<SkeletalMeshAsset>();
    PerMatIndices perMat;
    outUsedMaterials.assign(scene->mNumMaterials, 0);

    // 스킨 트래버스도 static처럼 global 누적해서 bake
    TraverseAndBuild_Skinned_PerMaterial_Rebuild(scene, scene->mRootNode, aiMatrix4x4(), *sk, perMat, outUsedMaterials);

    FbxImportUtils::FinalizeIndexBufferAndSections(perMat, outUsedMaterials, *sk);

    if (sk->Vertices.empty() || sk->Indices.empty() || sk->Sections.empty())
        return false;

    // 부모관계 구성
    BuildBoneParentsFromScene(scene, sk->Skeleton);

    // bind pose palette 생성
    BuildBindPoseBonePalette_Rebuild(scene, *sk);
    
    BuildNodeTreeAndBoneMapping_Assimp(scene, sk->Skeleton);
    
    // 기본 포즈 구성
    BuildRefLocalPoseFromScene(scene, sk->Skeleton, false);
    aiMatrix4x4 I = scene->mRootNode->mTransformation;
    I.Inverse();
    sk->Skeleton.GlobalInverse = ToXM4(I); 

    outMesh = std::move(sk);
    return true;
}

bool FbxImporter::ImportFBX(const std::string& path, const ImportOptions& opt, ImportResult& out)
{
    out = {};

    Assimp::Importer importer;

    unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType;

    if (opt.bGenerateNormalsIfMissing) flags |= aiProcess_GenSmoothNormals;
    if (opt.bGenerateTangents)         flags |= aiProcess_CalcTangentSpace;
    if (opt.bFlipUV)                   flags |= aiProcess_FlipUVs;
    if (opt.bConvertToLeftHanded)      flags |= aiProcess_ConvertToLeftHanded;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene)
    {
        OutputDebugStringA(importer.GetErrorString());
        OutputDebugStringA("\n");
        return false;
    }
    if (!scene->HasMeshes() || !scene->mRootNode)
    {
        OutputDebugStringA("Assimp loaded scene but no meshes/root.\n");
        return false;
    }

    ID3D11Device* device = Dx11Context::Get().GetDevice();
    if (!device) return false;

    const std::wstring baseDir = GetBaseDirW(Utf8ToWide(path));
    std::vector<uint8_t> usedMaterials(scene->mNumMaterials, 0);

    const bool bSkeletal = SceneHasSkinnedMesh(scene);

    
    if (bSkeletal)
    {
        std::shared_ptr<SkeletalMeshAsset> sk;
        if (!BuildSkeletal(scene, opt, sk, usedMaterials))
            return false;

        out.Type = EImportedMeshType::Skeletal;
        out.SkeletalMesh = std::move(sk);
    }
    else
    {
        std::shared_ptr<MeshAsset> st;
        if (!BuildStatic(scene, opt, st, usedMaterials))
            return false;

        out.Type = EImportedMeshType::Static;
        out.StaticMesh = std::move(st);
    }

    out.Materials = FbxMaterialBuilder::BuildMaterialsByMatIndex(device, scene, baseDir, usedMaterials);
    return true;
}
