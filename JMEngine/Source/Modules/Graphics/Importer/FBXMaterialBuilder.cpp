#include "FbxMaterialBuilder.h"

#include <DirectXTK/WICTextureLoader.h>
#include <filesystem>
#include <assimp/pbrmaterial.h>

#include "../Dx11Context.h" // GetSharedBasicMaterial()
#include "../../Game/MeshData.h" // MaterialInstance가 여기에 있다면 include 조정

using namespace FbxImportUtils;
using Microsoft::WRL::ComPtr;

namespace
{
    static ComPtr<ID3D11ShaderResourceView> CreateSRVFromEmbeddedRawBGRA8(ID3D11Device* device, const aiTexture* at)
    {
        ComPtr<ID3D11ShaderResourceView> srv;
        if (!device || !at || at->mWidth == 0 || at->mHeight == 0) return srv;

        D3D11_TEXTURE2D_DESC td{};
        td.Width = at->mWidth;
        td.Height = at->mHeight;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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

    static ComPtr<ID3D11ShaderResourceView> LoadTextureSRV_Internal(
        ID3D11Device* device,
        const aiScene* scene,
        const aiString& texPath,
        const std::wstring& baseDir,
        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
        const ComPtr<ID3D11ShaderResourceView>& fallbackSRV,
        bool bSRGB)
    {
        if (!device || !scene || texPath.length == 0)
            return fallbackSRV;

        std::string t8 = texPath.C_Str();
        if (t8.empty())
            return fallbackSRV;

        // 1) embedded
        if (const aiTexture* at = scene->GetEmbeddedTexture(t8.c_str()))
        {
            // compressed
            if (at->mHeight == 0)
            {
                ComPtr<ID3D11Resource> res;
                ID3D11ShaderResourceView* raw = nullptr;
                if (SUCCEEDED(DirectX::CreateWICTextureFromMemory(
                    device,
                    reinterpret_cast<const uint8_t*>(at->pcData),
                    at->mWidth,
                    res.GetAddressOf(),
                    &raw)))
                {
                    ComPtr<ID3D11ShaderResourceView> srv;
                    srv.Attach(raw);
                    return srv;
                }
            }

            auto srv = CreateSRVFromEmbeddedRawBGRA8(device, at);
            return srv ? srv : fallbackSRV;
        }

        // 2) external file
        std::wstring wtex = NormalizeTexPathFromAssimp(texPath);
        if (wtex.empty())
            return fallbackSRV;

        std::wstring full = IsAbsPathW(wtex) ? wtex : (baseDir + wtex);

        if (auto it = fileCache.find(full); it != fileCache.end() && it->second)
            return it->second;

        if (!std::filesystem::exists(full))
            return fallbackSRV;

        DirectX::WIC_LOADER_FLAGS wicFlags = DirectX::WIC_LOADER_DEFAULT;
        if (bSRGB) wicFlags = (DirectX::WIC_LOADER_FLAGS)(wicFlags | DirectX::WIC_LOADER_FORCE_SRGB);
        else       wicFlags = (DirectX::WIC_LOADER_FLAGS)(wicFlags | DirectX::WIC_LOADER_IGNORE_SRGB);

        ComPtr<ID3D11Resource> res;
        ID3D11ShaderResourceView* raw = nullptr;

        HRESULT hr = DirectX::CreateWICTextureFromFileEx(
            device,
            nullptr,
            full.c_str(),
            0,
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE,
            0,
            0,
            wicFlags,
            res.GetAddressOf(),
            &raw
        );

        if (SUCCEEDED(hr))
        {
            ComPtr<ID3D11ShaderResourceView> srv;
            srv.Attach(raw);
            fileCache[full] = srv;
            return srv;
        }

        return fallbackSRV;
    }

    static ComPtr<ID3D11ShaderResourceView> LoadBaseColorSRV(
        ID3D11Device* device,
        const aiScene* scene,
        aiMaterial* mat,
        const std::wstring& baseDir,
        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
        ComPtr<ID3D11ShaderResourceView>& whiteSRV)
    {
        CreateFallbackWhiteSRV(device, whiteSRV, true);
        if (!scene || !mat) return whiteSRV;

        aiString texPath;
        if (!GetFirstTexturePath(mat, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, texPath))
            return whiteSRV;

        return LoadTextureSRV_Internal(device, scene, texPath, baseDir, fileCache, whiteSRV, true);
    }

    static ComPtr<ID3D11ShaderResourceView> LoadNormalSRV(
        ID3D11Device* device,
        const aiScene* scene,
        aiMaterial* mat,
        const std::wstring& baseDir,
        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
        ComPtr<ID3D11ShaderResourceView>& flatNormalSRV)
    {
        CreateFallbackFlatNormalSRV(device, flatNormalSRV);
        if (!scene || !mat) return flatNormalSRV;

        aiString texPath;
        if (!GetFirstTexturePath(mat, { aiTextureType_NORMALS, aiTextureType_HEIGHT }, texPath))
            return flatNormalSRV;

        return LoadTextureSRV_Internal(device, scene, texPath, baseDir, fileCache, flatNormalSRV, false);
    }

    static ComPtr<ID3D11ShaderResourceView> LoadEmissiveSRV(
        ID3D11Device* device,
        const aiScene* scene,
        aiMaterial* mat,
        const std::wstring& baseDir,
        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
        ComPtr<ID3D11ShaderResourceView>& blackSRV)
    {
        CreateFallbackBlackSRV(device, blackSRV, true);
        if (!scene || !mat) return blackSRV;

        aiString texPath;
        if (!GetFirstTexturePath(mat, { aiTextureType_EMISSIVE }, texPath))
            return blackSRV;

        return LoadTextureSRV_Internal(device, scene, texPath, baseDir, fileCache, blackSRV, true);
    }

    static ComPtr<ID3D11ShaderResourceView> LoadAOSRV(
        ID3D11Device* device,
        const aiScene* scene,
        aiMaterial* mat,
        const std::wstring& baseDir,
        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
        ComPtr<ID3D11ShaderResourceView>& whiteSRV)
    {
        CreateFallbackWhiteSRV(device, whiteSRV, false);
        if (!scene || !mat) return whiteSRV;

        aiString texPath;
        if (!GetFirstTexturePath(mat, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP }, texPath))
            return whiteSRV;

        return LoadTextureSRV_Internal(device, scene, texPath, baseDir, fileCache, whiteSRV, false);
    }

    static FbxMaterialBuilder::FMetalRoughSRVs LoadMetalRoughSRVs(
        ID3D11Device* device,
        const aiScene* scene,
        aiMaterial* mat,
        const std::wstring& baseDir,
        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>>& fileCache,
        ComPtr<ID3D11ShaderResourceView>& blackSRV,
        ComPtr<ID3D11ShaderResourceView>& whiteSRV)
    {
        CreateFallbackBlackSRV(device, blackSRV, false);
        CreateFallbackWhiteSRV(device, whiteSRV, false);

        FbxMaterialBuilder::FMetalRoughSRVs out;
        out.MetallicSRV = blackSRV;
        out.RoughnessSRV = whiteSRV;

        if (!scene || !mat) return out;

        aiString metalPath, roughPath;

        const bool bHasMetal = GetFirstTexturePath(mat, { aiTextureType_METALNESS }, metalPath);
        const bool bHasRough = GetFirstTexturePath(mat, { aiTextureType_DIFFUSE_ROUGHNESS }, roughPath);

        if (bHasMetal)
            out.MetallicSRV = LoadTextureSRV_Internal(device, scene, metalPath, baseDir, fileCache, blackSRV, false);
        if (bHasRough)
            out.RoughnessSRV = LoadTextureSRV_Internal(device, scene, roughPath, baseDir, fileCache, whiteSRV, false);

        if (!bHasMetal && !bHasRough)
        {
            aiString unk;
            if (GetFirstTexturePath(mat, { aiTextureType_UNKNOWN }, unk))
            {
                auto packed = LoadTextureSRV_Internal(device, scene, unk, baseDir, fileCache, whiteSRV, false);
                out.MetallicSRV = packed ? packed : blackSRV;
                out.RoughnessSRV = packed ? packed : whiteSRV;
                out.bPackedMR_GB = true;
            }
        }
        else if (metalPath == roughPath)
        {
            out.bPackedMR_GB = true;
        }

        return out;
    }
}

namespace FbxMaterialBuilder
{
    std::vector<std::shared_ptr<MaterialInstance>> BuildMaterialsByMatIndex(
        ID3D11Device* device,
        const aiScene* scene,
        const std::wstring& baseDir,
        const std::vector<uint8_t>& usedMaterials)
    {
        std::vector<std::shared_ptr<MaterialInstance>> out;
        out.resize(scene ? scene->mNumMaterials : 0);

        auto baseMat = Dx11Context::Get().GetSharedBasicMaterial();
        if (!baseMat || !scene) return out;

        std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>> texCache;

        ComPtr<ID3D11ShaderResourceView> whiteSRV;
        ComPtr<ID3D11ShaderResourceView> blackSRV;
        ComPtr<ID3D11ShaderResourceView> flatNormalSRV;

        for (uint32_t mi = 0; mi < scene->mNumMaterials; ++mi)
        {
            if (mi < usedMaterials.size() && !usedMaterials[mi]) continue;

            aiMaterial* mat = scene->mMaterials[mi];
            if (!mat) continue;

            auto inst = std::make_shared<MaterialInstance>(baseMat);

            // BaseColorFactor
            {
                aiColor4D base(1, 1, 1, 1);
                if (mat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, base) == AI_SUCCESS ||
                    aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &base) == AI_SUCCESS)
                {
                    inst->SetBaseColor({ base.r, base.g, base.b, base.a });
                }
            }

            // Metallic/Roughness factor
            {
                float metallic = 0.0f;
                float roughness = 1.0f;

                mat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
                mat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);

                inst->SetMetallic(metallic);
                inst->SetRoughness(roughness);
            }

            // EmissiveFactor
            {
                aiColor3D e(0, 0, 0);
                if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, e) == AI_SUCCESS)
                    inst->SetEmissive({ e.r, e.g, e.b });
            }

            // Textures
            {
                auto baseSrv = LoadBaseColorSRV(device, scene, mat, baseDir, texCache, whiteSRV);
                inst->SetTexture(0, baseSrv.Get());
            }
            {
                auto normalSrv = LoadNormalSRV(device, scene, mat, baseDir, texCache, flatNormalSRV);
                inst->SetTexture(1, normalSrv.Get());
                inst->SetUseNormalMap(true);
            }
            {
                auto mr = LoadMetalRoughSRVs(device, scene, mat, baseDir, texCache, blackSRV, whiteSRV);
                inst->SetTexture(2, mr.MetallicSRV.Get());
                if (!mr.bPackedMR_GB)
                    inst->SetTexture(5, mr.RoughnessSRV.Get());
                inst->SetUsePackedMetalRough(mr.bPackedMR_GB);
            }
            {
                auto aoSrv = LoadAOSRV(device, scene, mat, baseDir, texCache, whiteSRV);
                inst->SetTexture(3, aoSrv.Get());
            }
            {
                auto emSrv = LoadEmissiveSRV(device, scene, mat, baseDir, texCache, blackSRV);
                inst->SetTexture(4, emSrv.Get());
            }

            out[mi] = std::move(inst);
        }

        return out;
    }
}
