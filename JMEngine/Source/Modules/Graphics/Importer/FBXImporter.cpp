#include "FbxImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXTK/WICTextureLoader.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

#include "../Dx11Context.h"
#include "../../Game/MeshData.h"

using Microsoft::WRL::ComPtr;
using PerMatIndices = std::unordered_map<uint32_t, std::vector<uint32_t>>;

static DirectX::XMFLOAT3 ToXM3(const aiVector3D& v) { return { v.x, v.y, v.z }; }
static DirectX::XMFLOAT2 ToXM2(const aiVector3D& v) { return { v.x, v.y }; }

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), sz);
    return out;
}

static std::wstring GetBaseDirW(const std::wstring& filePath)
{
    const size_t pos = filePath.find_last_of(L"/\\");
    if (pos == std::wstring::npos) return L"";
    return filePath.substr(0, pos + 1);
}

static bool IsAbsPathW(const std::wstring& p)
{
    if (p.empty()) return false;
    if (p.find(L":") != std::wstring::npos) return true;
    if (p.rfind(L"\\\\", 0) == 0) return true;
    if (p[0] == L'/' || p[0] == L'\\') return true;
    return false;
}

static std::wstring NormalizeTexPathFromAssimp(const aiString& texPath)
{
    // Assimp가 주는 경로는 UTF-8일 수 있음
    std::string u8 = texPath.C_Str();
    return Utf8ToWide(u8);
}

// aiMatrix4x4 -> (pos transform, normal transform용)
// aiMatrix4x4는 row/col이 섞일 수 있으니, Assimp 곱셈 연산자 이용이 안전함.
// pos: p' = global * p
// normal: (inverse-transpose of upper3x3)
static aiMatrix3x3 MakeNormalMatrix(const aiMatrix4x4& m)
{
    aiMatrix3x3 n(m);
    n.Inverse();
    n.Transpose();
    return n;
}

static void AppendMeshTransformed_PerMaterial(
    const aiScene* scene,
    const aiMesh* mesh,
    const aiMatrix4x4& global,
    MeshAsset& outAsset,
    PerMatIndices& outPerMatIndices,
    std::vector<uint8_t>& outUsedMaterials // size = scene->mNumMaterials, 0/1
)
{
    if (!mesh) return;

    const aiMatrix3x3 normalMat = MakeNormalMatrix(global);

    const bool hasNormals = mesh->HasNormals();
    const bool hasUV0 = mesh->HasTextureCoords(0);
    const bool hasUV1 = mesh->HasTextureCoords(1); // (옵션) UV0 없을 때 UV1 사용

    const int uvCh = hasUV0 ? 0 : (hasUV1 ? 1 : -1);

    const uint32_t baseV = (uint32_t)outAsset.Vertices.size();
    outAsset.Vertices.reserve(outAsset.Vertices.size() + mesh->mNumVertices);

    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex v{};

        aiVector3D p = mesh->mVertices[i];
        p = global * p;
        v.Pos = ToXM3(p);

        aiVector3D n = hasNormals ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
        n = normalMat * n;
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

        outAsset.Vertices.push_back(v);
    }

    // mesh의 material index
    const uint32_t matIndex = mesh->mMaterialIndex;
    if (matIndex < outUsedMaterials.size())
        outUsedMaterials[matIndex] = 1;

    // Triangulate 했으니 face는 3개 인덱스
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

static void TraverseAndBuild_PerMaterial(
    const aiScene* scene,
    const aiNode* node,
    const aiMatrix4x4& parent,
    MeshAsset& outAsset,
    PerMatIndices& outPerMatIndices,
    std::vector<uint8_t>& outUsedMaterials // size=scene->mNumMaterials
)
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
        TraverseAndBuild_PerMaterial(scene, node->mChildren[ci], global, outAsset, outPerMatIndices, outUsedMaterials);
}

// per-material indices -> 하나의 index buffer + sections 생성
static void FinalizeIndexBufferAndSections(
    const aiScene* scene,
    PerMatIndices& perMat,
    const std::vector<uint8_t>& usedMaterials,
    MeshAsset& outAsset
)
{
    outAsset.Indices.clear();
    outAsset.Sections.clear();

    // materialIndex를 오름차순으로 안정적으로 처리
    // (언리얼도 기본적으로 슬롯 순서 기준으로 섹션/드로우가 정렬됨)
    for (uint32_t matIndex = 0; matIndex < (uint32_t)usedMaterials.size(); ++matIndex)
    {
        if (!usedMaterials[matIndex]) continue;

        auto it = perMat.find(matIndex);
        if (it == perMat.end() || it->second.empty()) continue;

        MeshSection sec{};
        sec.MaterialIndex = matIndex;
        sec.StartIndex = (uint32_t)outAsset.Indices.size();
        sec.IndexCount = (uint32_t)it->second.size();

        outAsset.Indices.insert(outAsset.Indices.end(), it->second.begin(), it->second.end());
        outAsset.Sections.push_back(sec);
    }
}

static void Create1x1WhiteSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& outSRV)
{
    if (outSRV) return;

    UINT white = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = &white;
    sd.SysMemPitch = sizeof(UINT);

    ComPtr<ID3D11Texture2D> tex;
    if (SUCCEEDED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format = td.Format;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;
        srvd.Texture2D.MostDetailedMip = 0;
        device->CreateShaderResourceView(tex.Get(), &srvd, outSRV.GetAddressOf());
    }
}

// embedded texture (raw aiTexel) SRV 생성
static ComPtr<ID3D11ShaderResourceView> CreateSRVFromEmbeddedRawBGRA8(
    ID3D11Device* device, const aiTexture* at)
{
    ComPtr<ID3D11ShaderResourceView> srv;
    if (!device || !at || at->mWidth == 0 || at->mHeight == 0) return srv;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = at->mWidth;
    td.Height = at->mHeight;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 글과 동일
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = at->pcData;
    sd.SysMemPitch = at->mWidth * sizeof(aiTexel);

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
        return srv;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    srvd.Texture2D.MostDetailedMip = 0;

    device->CreateShaderResourceView(tex.Get(), &srvd, srv.GetAddressOf());
    return srv;
}

// 텍스처 로딩(embedded / *index / external relative/abs)
// - 캐시 포함(파일 경로 기준)
static ComPtr<ID3D11ShaderResourceView> LoadDiffuseSRV(
    ID3D11Device* device,
    const aiScene* scene,
    aiMaterial* mat,
    const std::wstring& baseDir,
    std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
    ComPtr<ID3D11ShaderResourceView>& whiteSRV)
{
    Create1x1WhiteSRV(device, whiteSRV);
    if (!scene || !mat) return whiteSRV;

    aiString texPath;
    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) != AI_SUCCESS)
        return whiteSRV;

    std::string t8 = texPath.C_Str();
    if (t8.empty())
        return whiteSRV;

    // 1) embedded (이름 또는 "*0" 형태도 GetEmbeddedTexture가 처리해줌)
    if (const aiTexture* at = scene->GetEmbeddedTexture(t8.c_str()))
    {
        // 압축 이미지(PNG/JPG): mHeight == 0, mWidth == 압축 데이터 길이
        if (at->mHeight == 0)
        {
            ComPtr<ID3D11Resource> res;
            ID3D11ShaderResourceView* raw = nullptr;
            if (SUCCEEDED(DirectX::CreateWICTextureFromMemory(
                device,
                reinterpret_cast<const uint8_t*>(at->pcData),
                at->mWidth, // 데이터 길이
                res.GetAddressOf(),
                &raw)))
            {
                ComPtr<ID3D11ShaderResourceView> srv;
                srv.Attach(raw);
                return srv;
            }
        }
        // RAW BGRA8
        auto srv = CreateSRVFromEmbeddedRawBGRA8(device, at);
        return srv ? srv : whiteSRV;
    }

    // 2) 구형 "*index" 접근
    if (!t8.empty() && t8[0] == '*')
    {
        int idx = atoi(t8.c_str() + 1);
        if (idx >= 0 && (unsigned)idx < scene->mNumTextures)
        {
            const aiTexture* at = scene->mTextures[idx];
            if (at)
            {
                if (at->mHeight == 0)
                {
                    ComPtr<ID3D11Resource> res;
                    ID3D11ShaderResourceView* raw = nullptr;
                    if (SUCCEEDED(DirectX::CreateWICTextureFromMemory(device,
                        (const uint8_t*)at->pcData, at->mWidth, res.GetAddressOf(), &raw)))
                    {
                        ComPtr<ID3D11ShaderResourceView> srv;
                        srv.Attach(raw);
                        return srv;
                    }
                    return whiteSRV;
                }

                auto srv = CreateSRVFromEmbeddedRawBGRA8(device, at);
                return srv ? srv : whiteSRV;
            }
        }
    }

    // 3) 외부 파일(absolute / relative)
    std::wstring wtex = NormalizeTexPathFromAssimp(texPath);
    if (wtex.empty()) return whiteSRV;

    std::wstring full = IsAbsPathW(wtex) ? wtex : (baseDir + wtex);

    // 캐시
    if (auto it = fileCache.find(full); it != fileCache.end() && it->second)
        return it->second;

    // 파일 존재 확인
    if (!std::filesystem::exists(full))
    {
        return whiteSRV;
    }

    // DirectXTK로 로드
    ComPtr<ID3D11Resource> res;
    ID3D11ShaderResourceView* raw = nullptr;
    if (SUCCEEDED(DirectX::CreateWICTextureFromFile(device, full.c_str(), res.GetAddressOf(), &raw)))
    {
        ComPtr<ID3D11ShaderResourceView> srv;
        srv.Attach(raw);
        fileCache[full] = srv;
        return srv;
    }
    
    // 실패 시 white.
    return whiteSRV;
}

static void AppendMeshTransformed(const aiScene* scene, const aiMesh* mesh, const aiMatrix4x4& global, MeshAsset& outAsset)
{
    if (!mesh) return;

    const aiMatrix3x3 normalMat = MakeNormalMatrix(global);

    const bool hasNormals = mesh->HasNormals();
    const bool hasUV0 = mesh->HasTextureCoords(0);

    const uint32_t baseV = (uint32_t)outAsset.Vertices.size();
    outAsset.Vertices.reserve(outAsset.Vertices.size() + mesh->mNumVertices);

    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex v{};

        aiVector3D p = mesh->mVertices[i];
        p = global * p;
        v.Pos = ToXM3(p);

        aiVector3D n = hasNormals ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
        n = normalMat * n;
        n.Normalize();
        v.Normal = ToXM3(n);

        if (hasUV0)
        {
            const aiVector3D uv = mesh->mTextureCoords[0][i];
            v.UV = ToXM2(uv);
        }
        else
        {
            v.UV = { 0, 0 };
        }

        outAsset.Vertices.push_back(v);
    }

    outAsset.Indices.reserve(outAsset.Indices.size() + mesh->mNumFaces * 3);
    for (unsigned f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;

        outAsset.Indices.push_back(baseV + (uint32_t)face.mIndices[0]);
        outAsset.Indices.push_back(baseV + (uint32_t)face.mIndices[1]);
        outAsset.Indices.push_back(baseV + (uint32_t)face.mIndices[2]);
    }
}

static void TraverseAndBuild(
    const aiScene* scene,
    const aiNode* node,
    const aiMatrix4x4& parent,
    MeshAsset& outAsset,
    int& inOutFirstMaterialIndex)
{
    aiMatrix4x4 global = parent * node->mTransformation;

    for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
    {
        const unsigned meshIndex = node->mMeshes[mi];
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) continue;

        if (inOutFirstMaterialIndex < 0)
            inOutFirstMaterialIndex = (int)mesh->mMaterialIndex;

        AppendMeshTransformed(scene, mesh, global, outAsset);
    }

    for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        TraverseAndBuild(scene, node->mChildren[ci], global, outAsset, inOutFirstMaterialIndex);
}

// 가장 큰(mesh faces) 하나만 고르는 경우를 지원하려면 node 탐색이 필요
static const aiNode* FindNodeForMesh(const aiNode* node, unsigned meshIndex)
{
    for (unsigned i = 0; i < node->mNumMeshes; ++i)
        if (node->mMeshes[i] == meshIndex) return node;

    for (unsigned c = 0; c < node->mNumChildren; ++c)
        if (const aiNode* found = FindNodeForMesh(node->mChildren[c], meshIndex))
            return found;

    return nullptr;
}

static aiMatrix4x4 GetGlobalTransform(const aiNode* node)
{
    aiMatrix4x4 m; // identity
    const aiNode* cur = node;
    while (cur)
    {
        m = cur->mTransformation * m;
        cur = cur->mParent;
    }
    return m;
}

static int ChooseBestMeshIndex(const aiScene* scene)
{
    int best = -1;
    unsigned bestFaces = 0;
    for (unsigned i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* m = scene->mMeshes[i];
        if (!m || m->mNumVertices == 0 || m->mNumFaces == 0) continue;
        if (m->mNumFaces > bestFaces)
        {
            bestFaces = m->mNumFaces;
            best = (int)i;
        }
    }
    return best;
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
    if (!scene || !scene->HasMeshes() || !scene->mRootNode)
        return false;

    ID3D11Device* device = Dx11Context::Get().GetDevice();
    if (!device) return false;

    const std::wstring pathW = Utf8ToWide(path);
    const std::wstring baseDir = GetBaseDirW(pathW);

    // 1) 메시 머지 + per-material indices 수집
    auto meshAsset = std::make_shared<MeshAsset>();
    meshAsset->Stride = sizeof(Vertex);

    PerMatIndices perMat;
    std::vector<uint8_t> usedMaterials(scene->mNumMaterials, 0);

    // 여기서는 “전부 merge” 버전 (opt.bMergeAllMeshes가 true일 때)
    // (best mesh only도 원하면 별도 처리 가능)
    TraverseAndBuild_PerMaterial(scene, scene->mRootNode, aiMatrix4x4(), *meshAsset, perMat, usedMaterials);

    // 2) indices+sections finalize
    FinalizeIndexBufferAndSections(scene, perMat, usedMaterials, *meshAsset);

    if (meshAsset->Vertices.empty() || meshAsset->Indices.empty() || meshAsset->Sections.empty())
        return false;

    // 3) matIndex별로 Materials 생성
    auto materials = BuildMaterialsByMatIndex(device, scene, baseDir, usedMaterials);

    out.Mesh = std::move(meshAsset);
    out.Materials = std::move(materials);
    return true;
}

std::shared_ptr<MeshAsset> FbxImporter::BuildMeshAssetFromScene(const aiScene* scene, const ImportOptions& opt, int& outPickedMaterialIndex)
{
    outPickedMaterialIndex = -1;

    auto meshAsset = std::make_shared<MeshAsset>();
    meshAsset->Stride = sizeof(Vertex);

    if (opt.bMergeAllMeshes)
    {
        // node traverse해서 global transform 적용 + 전부 merge
        TraverseAndBuild(scene, scene->mRootNode, aiMatrix4x4(), *meshAsset, outPickedMaterialIndex);
    }
    else
    {
        // 큰 mesh 하나만 선택
        const int bestIdx = ChooseBestMeshIndex(scene);
        if (bestIdx < 0) return nullptr;

        const aiMesh* mesh = scene->mMeshes[bestIdx];
        if (!mesh || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) return nullptr;

        const aiNode* node = FindNodeForMesh(scene->mRootNode, (unsigned)bestIdx);
        aiMatrix4x4 global = node ? GetGlobalTransform(node) : aiMatrix4x4();

        outPickedMaterialIndex = (int)mesh->mMaterialIndex;
        AppendMeshTransformed(scene, mesh, global, *meshAsset);
    }

    return meshAsset;
}

std::vector<std::shared_ptr<MaterialInstance>> FbxImporter::BuildMaterialsByMatIndex(ID3D11Device* device,const aiScene* scene, const std::wstring& baseDir, const std::vector<uint8_t>& usedMaterials
)
{
    std::vector<std::shared_ptr<MaterialInstance>> out;
    out.resize(scene->mNumMaterials);

    auto baseMat = Dx11Context::Get().GetSharedBasicMaterial();
    if (!baseMat) return out;

    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texCache;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV;
    Create1x1WhiteSRV(device, whiteSRV);

    for (uint32_t mi = 0; mi < scene->mNumMaterials; ++mi)
    {
        if (!usedMaterials[mi]) continue;

        aiMaterial* mat = scene->mMaterials[mi];
        if (!mat) continue;

        auto inst = std::make_shared<MaterialInstance>(baseMat);

        // diffuse/basecolor color
        aiColor4D diffuse(1, 1, 1, 1);
        if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
        {
            inst->SetBaseColor({ diffuse.r, diffuse.g, diffuse.b, diffuse.a });
        }

        // (nonPBR이면 shininess -> roughness 근사도 가능)
        // float shininess = 0.f;
        // if (AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_SHININESS, &shininess))
        //     inst->SetRoughness(std::clamp(std::sqrt(2.f / (shininess + 2.f)), 0.02f, 1.f));

        // diffuse/basecolor texture slot0
        auto srv = LoadDiffuseSRV(device, scene, mat, baseDir, texCache, whiteSRV);
        inst->SetTexture(0, srv.Get());

        out[mi] = std::move(inst);
    }

    return out;
}
