#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <wrl/client.h>

#include <assimp/matrix4x4.h>
#include <assimp/matrix3x3.h>
#include <assimp/scene.h>

#include <DirectXMath.h>
#include <d3d11.h>

#include "FBXImportType.h"

struct Skeleton;
struct SkinnedVertex;

namespace FbxImportUtils
{
    using Microsoft::WRL::ComPtr;
    using PerMatIndices = std::unordered_map<uint32_t, std::vector<uint32_t>>;

    // -----------------------------
    // small helpers
    // -----------------------------
    DirectX::XMFLOAT3 ToXM3(const aiVector3D& v);
    DirectX::XMFLOAT2 ToXM2(const aiVector3D& v);

    std::wstring Utf8ToWide(const std::string& s);
    std::wstring GetBaseDirW(const std::wstring& filePath);
    bool IsAbsPathW(const std::wstring& p);
    std::wstring NormalizeTexPathFromAssimp(const aiString& texPath);

    aiMatrix3x3 MakeNormalMatrix(const aiMatrix4x4& m);
    DirectX::XMFLOAT4X4 ToXM4(const aiMatrix4x4& m);

    // -----------------------------
    // per-material finalize (static/skeletal 공용)
    // -----------------------------
    template<class TMeshAsset>
    void FinalizeIndexBufferAndSections(
        PerMatIndices& perMat,
        const std::vector<uint8_t>& usedMaterials,
        TMeshAsset& outAsset)
    {
        outAsset.Indices.clear();
        outAsset.Sections.clear();

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

    // -----------------------------
    // skeletal helpers
    // -----------------------------
    void AddBoneData(SkinnedVertex& v, uint32_t boneIndex, float weight);
    void NormalizeWeights(SkinnedVertex& v);

    const aiNode* FindNodeByName(const aiNode* node, const std::string& name);
    void BuildBoneParentsFromScene(const aiScene* scene, Skeleton& skel);

    bool SceneHasSkinnedMesh(const aiScene* scene);

    // -----------------------------
    // fallback SRV
    // -----------------------------
    void Create1x1SolidSRV(
        ID3D11Device* device,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a,
        ComPtr<ID3D11ShaderResourceView>& outSRV,
        bool sRGB);

    void CreateFallbackWhiteSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& whiteSRV, bool sRGB);
    void CreateFallbackBlackSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& blackSRV, bool sRGB);
    void CreateFallbackFlatNormalSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& flatNormalSRV);

    // -----------------------------
    // material texture path helper
    // -----------------------------
    bool GetFirstTexturePath(const aiMaterial* mat, const std::initializer_list<aiTextureType>& types, aiString& outPath);
}
