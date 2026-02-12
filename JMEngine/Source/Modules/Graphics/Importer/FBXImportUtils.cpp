#include "FbxImportUtils.h"

#include <Windows.h>
#include <filesystem>
#include <cmath>

#include "../../Game/Skeletal/SkeletalMeshData.h"

using namespace DirectX;

namespace FbxImportUtils
{
    XMFLOAT3 ToXM3(const aiVector3D& v) { return { v.x, v.y, v.z }; }
    XMFLOAT2 ToXM2(const aiVector3D& v) { return { v.x, v.y }; }

    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(sz, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), sz);
        return out;
    }

    std::wstring GetBaseDirW(const std::wstring& filePath)
    {
        const size_t pos = filePath.find_last_of(L"/\\");
        if (pos == std::wstring::npos) return L"";
        return filePath.substr(0, pos + 1);
    }

    bool IsAbsPathW(const std::wstring& p)
    {
        if (p.empty()) return false;
        if (p.find(L":") != std::wstring::npos) return true;
        if (p.rfind(L"\\\\", 0) == 0) return true;
        if (p[0] == L'/' || p[0] == L'\\') return true;
        return false;
    }

    std::wstring NormalizeTexPathFromAssimp(const aiString& texPath)
    {
        return Utf8ToWide(texPath.C_Str());
    }

    aiMatrix3x3 MakeNormalMatrix(const aiMatrix4x4& m)
    {
        aiMatrix3x3 n(m);
        n.Inverse();
        n.Transpose();
        return n;
    }

    // XMFLOAT4X4 ToXM4(const aiMatrix4x4& m)
    // {
    //     return XMFLOAT4X4(
    //         m.a1, m.b1, m.c1, m.d1,
    //         m.a2, m.b2, m.c2, m.d2,
    //         m.a3, m.b3, m.c3, m.d3,
    //         m.a4, m.b4, m.c4, m.d4
    //     );
    // }
    
    XMFLOAT4X4 ToXM4(const aiMatrix4x4& m)
    {
        return XMFLOAT4X4(
            m.a1, m.a2, m.a3, m.a4,
            m.b1, m.b2, m.b3, m.b4,
            m.c1, m.c2, m.c3, m.c4,
            m.d1, m.d2, m.d3, m.d4
        );
    }

    void AddBoneData(SkinnedVertex& v, uint32_t boneIndex, float weight)
    {
           if (weight <= 0.0f) return;

        // 1) 이미 같은 boneIndex가 있으면 weight 합치기
        for (int i = 0; i < 4; ++i)
        {
            if (v.BoneWeight[i] > 0.0f && v.BoneIndex[i] == boneIndex)
            {
                v.BoneWeight[i] += weight;
                return;
            }
        }

        // 2) 빈 슬롯 찾기(<=epsilon)
        for (int i = 0; i < 4; ++i)
        {
            if (v.BoneWeight[i] <= 1e-8f)
            {
                v.BoneIndex[i]  = boneIndex;
                v.BoneWeight[i] = weight;
                return;
            }
        }

        // 3) 이미 4개 다 찼으면 가장 작은 weight를 교체
        int minI = 0;
        for (int i = 1; i < 4; ++i)
        {
            if (v.BoneWeight[i] < v.BoneWeight[minI])
            {
                minI = i;
            }
        }
            

        if (weight > v.BoneWeight[minI])
        {
            v.BoneIndex[minI]  = boneIndex;
            v.BoneWeight[minI] = weight;
        }
    }

    void NormalizeWeights(SkinnedVertex& v)
    {
        float sum = v.BoneWeight[0] + v.BoneWeight[1] + v.BoneWeight[2] + v.BoneWeight[3];
        if (sum <= 1e-8f)
        {
            v.BoneIndex[0] = 0; v.BoneWeight[0] = 1.0f;
            for (int i = 1; i < 4; ++i) { v.BoneIndex[i] = 0; v.BoneWeight[i] = 0.0f; }
            return;
        }
        float inv = 1.0f / sum;
        for (int i = 0; i < 4; ++i) v.BoneWeight[i] *= inv;
    }

    const aiNode* FindNodeByName(const aiNode* node, const std::string& name)
    {
        if (!node) return nullptr;
        if (name == node->mName.C_Str()) return node;
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            if (auto r = FindNodeByName(node->mChildren[i], name)) return r;
        return nullptr;
    }

    void BuildBoneParentsFromScene(const aiScene* scene, Skeleton& skel)
    {
        if (!scene || !scene->mRootNode) return;

        for (uint32_t i = 0; i < (uint32_t)skel.Bones.size(); ++i)
        {
            BoneInfo& b = skel.Bones[i];
            const aiNode* node = FindNodeByName(scene->mRootNode, b.Name);
            // 없으면 
            if (!node)
            {
                b.ParentIndex = -1; 
                continue;
            }

            int parentIndex = -1;
            const aiNode* p = node->mParent;
            while (p)
            {
                auto it = skel.BoneNameToIndex.find(p->mName.C_Str());
                if (it != skel.BoneNameToIndex.end())
                {
                    parentIndex = (int)it->second;
                    break;
                }
                p = p->mParent;
            }
            b.ParentIndex = parentIndex;
        }
    }

    bool SceneHasSkinnedMesh(const aiScene* scene)
    {
        if (!scene || !scene->HasMeshes()) return false;
        for (unsigned i = 0; i < scene->mNumMeshes; ++i)
        {
            const aiMesh* m = scene->mMeshes[i];
            if (m && m->HasBones() && m->mNumBones > 0) return true;
        }
        return false;
    }

    void Create1x1SolidSRV(
        ID3D11Device* device,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a,
        ComPtr<ID3D11ShaderResourceView>& outSRV,
        bool sRGB)
    {
        if (outSRV) return;

        uint32_t pixel = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = &pixel;
        init.SysMemPitch = 4;

        ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(device->CreateTexture2D(&desc, &init, tex.GetAddressOf())))
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = desc.Format;
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MipLevels = 1;
            device->CreateShaderResourceView(tex.Get(), &sd, outSRV.GetAddressOf());
        }
    }

    void CreateFallbackWhiteSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& whiteSRV, bool sRGB)
    {
        Create1x1SolidSRV(device, 255, 255, 255, 255, whiteSRV, sRGB);
    }

    void CreateFallbackBlackSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& blackSRV, bool sRGB)
    {
        Create1x1SolidSRV(device, 0, 0, 0, 255, blackSRV, sRGB);
    }

    void CreateFallbackFlatNormalSRV(ID3D11Device* device, ComPtr<ID3D11ShaderResourceView>& flatNormalSRV)
    {
        // (128,128,255)
        Create1x1SolidSRV(device, 128, 128, 255, 255, flatNormalSRV, false);
    }

    bool GetFirstTexturePath(const aiMaterial* mat, const std::initializer_list<aiTextureType>& types, aiString& outPath)
    {
        if (!mat) return false;

        for (aiTextureType t : types)
        {
            if (mat->GetTextureCount(t) > 0 && mat->GetTexture(t, 0, &outPath) == AI_SUCCESS)
            {
                if (outPath.length > 0) return true;
            }
        }
        return false;
    }
}
