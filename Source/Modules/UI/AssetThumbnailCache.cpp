#include "AssetThumbnailCache.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

#include <DirectXTex.h>
#include <DirectXTexEXR.h>

#include "../Graphics/Dx11Context.h"

namespace
{
    struct ThumbnailFileHeader
    {
        uint32_t magic = 0x31424854; // THB1
        uint16_t width = 0;
        uint16_t height = 0;
        uint32_t rgbSize = 0;
    };

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
        {
            return (char)std::tolower(c);
        });
        return s;
    }

    bool LoadCachedThumbnailTexture2D(
        ID3D11Device* device,
        const std::filesystem::path& cachePath,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv)
    {
        std::ifstream ifs(cachePath, std::ios::binary);
        if (!ifs)
        {
            return false;
        }

        ThumbnailFileHeader hdr{};
        ifs.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!ifs || hdr.magic != 0x31424854 || hdr.width == 0 || hdr.height == 0 || hdr.rgbSize == 0)
        {
            return false;
        }

        const size_t expected = (size_t)hdr.width * (size_t)hdr.height * 3;
        if (hdr.rgbSize != expected)
        {
            return false;
        }

        std::vector<uint8_t> rgb(expected);
        ifs.read(reinterpret_cast<char*>(rgb.data()), (std::streamsize)rgb.size());
        if (!ifs)
        {
            return false;
        }

        std::vector<uint8_t> rgba((size_t)hdr.width * (size_t)hdr.height * 4);
        for (size_t i = 0; i < (size_t)hdr.width * (size_t)hdr.height; ++i)
        {
            rgba[i * 4 + 0] = rgb[i * 3 + 0];
            rgba[i * 4 + 1] = rgb[i * 3 + 1];
            rgba[i * 4 + 2] = rgb[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = hdr.width;
        td.Height = hdr.height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = rgba.data();
        init.SysMemPitch = hdr.width * 4;

        HRESULT hr = device->CreateTexture2D(&td, &init, tex.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(tex.Get(), &sd, outSrv.GetAddressOf());
        return SUCCEEDED(hr) && outSrv;
    }

    bool LoadTexturePreviewSRV(const AssetMeta& meta, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv)
    {
        outSrv.Reset();

        ID3D11Device* device = Dx11Context::Get().GetDevice();
        if (!device)
        {
            return false;
        }

        if (meta.type != AssetType::Texture2D)
        {
            return false;
        }

        const auto itThumbPath = meta.tags.find("ThumbnailCachePath");
        const auto itThumbW = meta.tags.find("ThumbnailW");
        const auto itThumbH = meta.tags.find("ThumbnailH");
        if (itThumbPath != meta.tags.end() && itThumbW != meta.tags.end() && itThumbH != meta.tags.end())
        {
            const std::filesystem::path cachePath(itThumbPath->second);
            if (std::filesystem::exists(cachePath))
            {
                if (LoadCachedThumbnailTexture2D(device, cachePath, outSrv))
                {
                    return true;
                }
            }
        }

        const std::string imagePath = meta.sourcePath;
        const std::filesystem::path p(imagePath);
        if (!std::filesystem::exists(p))
        {
            return false;
        }

        const std::wstring wpath = p.wstring();
        const std::string ext = ToLower(p.extension().string());

        DirectX::ScratchImage image;
        DirectX::TexMetadata metadata{};
        HRESULT hr = E_FAIL;

        if (ext == ".dds")
        {
            hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
        }
        else if (ext == ".tga")
        {
            hr = DirectX::LoadFromTGAFile(wpath.c_str(), &metadata, image);
        }
        else if (ext == ".hdr")
        {
            hr = DirectX::LoadFromHDRFile(wpath.c_str(), &metadata, image);
        }
        else if (ext == ".exr")
        {
            hr = DirectX::LoadFromEXRFile(wpath.c_str(), &metadata, image);
        }
        else
        {
            hr = DirectX::LoadFromWICFile(wpath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
        }

        if (FAILED(hr))
        {
            return false;
        }

        const DirectX::Image* srcImage = image.GetImage(0, 0, 0);
        if (!srcImage)
        {
            return false;
        }

        DirectX::ScratchImage decompImage;
        const DirectX::Image* workingImage = srcImage;
        if (DirectX::IsCompressed(srcImage->format))
        {
            hr = DirectX::Decompress(*srcImage, DXGI_FORMAT_UNKNOWN, decompImage);
            if (FAILED(hr))
            {
                return false;
            }
            workingImage = decompImage.GetImage(0, 0, 0);
            if (!workingImage)
            {
                return false;
            }
        }

        DirectX::ScratchImage rgbaImage;
        if (workingImage->format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            hr = DirectX::Convert(*workingImage, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, rgbaImage);
            if (FAILED(hr))
            {
                return false;
            }
            workingImage = rgbaImage.GetImage(0, 0, 0);
            if (!workingImage)
            {
                return false;
            }
        }

        DirectX::ScratchImage resizedImage;
        hr = DirectX::Resize(*workingImage, 64, 64, DirectX::TEX_FILTER_DEFAULT, resizedImage);
        if (SUCCEEDED(hr))
        {
            const DirectX::Image* resized = resizedImage.GetImage(0, 0, 0);
            if (resized)
            {
                workingImage = resized;
            }
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = (UINT)workingImage->width;
        td.Height = (UINT)workingImage->height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = workingImage->pixels;
        init.SysMemPitch = (UINT)workingImage->rowPitch;

        hr = device->CreateTexture2D(&td, &init, tex.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(tex.Get(), &sd, outSrv.GetAddressOf());

        return SUCCEEDED(hr) && outSrv;
    }
}

const AssetThumbnailInfo& AssetThumbnailCache::GetOrCreate(const AssetMeta& meta)
{
    const uint64_t sig = BuildSignature(meta);

    auto it = m_Entries.find(meta.id);
    if (it != m_Entries.end())
    {
        if (it->second.signature == sig)
        {
            return it->second;
        }

        it->second.signature = sig;
        it->second.tint = GetTint(meta.type);
        it->second.badge = GetBadge(meta.type);
        LoadTexturePreviewSRV(meta, it->second.previewSRV);
        return it->second;
    }

    AssetThumbnailInfo info{};
    info.id = meta.id;
    info.signature = sig;
    info.tint = GetTint(meta.type);
    info.badge = GetBadge(meta.type);
    LoadTexturePreviewSRV(meta, info.previewSRV);

    auto [insertedIt, _] = m_Entries.emplace(meta.id, std::move(info));
    return insertedIt->second;
}

void AssetThumbnailCache::GarbageCollect(const std::vector<AssetID>& keepIds, size_t softLimit)
{
    if (m_Entries.empty())
    {
        return;
    }

    if (m_Entries.size() <= softLimit)
    {
        return;
    }

    std::unordered_map<AssetID, bool> keep;
    keep.reserve(keepIds.size());
    for (AssetID id : keepIds)
    {
        keep[id] = true;
    }

    for (auto it = m_Entries.begin(); it != m_Entries.end(); )
    {
        if (keep.find(it->first) == keep.end())
        {
            it = m_Entries.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

uint64_t AssetThumbnailCache::BuildSignature(const AssetMeta& meta)
{
    uint64_t sig = 1469598103934665603ull;
    auto mix = [&sig](uint64_t v)
    {
        sig ^= v;
        sig *= 1099511628211ull;
    };

    mix(static_cast<uint64_t>(meta.type));
    mix(meta.sourceTimestamp);
    mix(meta.sourceSize);
    mix(std::hash<std::string>{}(meta.virtualPath));
    return sig;
}

ImVec4 AssetThumbnailCache::GetTint(AssetType type)
{
    switch (type)
    {
    case AssetType::Texture2D:    return ImVec4(0.20f, 0.45f, 0.20f, 1.0f);
    case AssetType::StaticMesh:   return ImVec4(0.22f, 0.32f, 0.50f, 1.0f);
    case AssetType::SkeletalMesh: return ImVec4(0.38f, 0.26f, 0.54f, 1.0f);
    case AssetType::Animation:    return ImVec4(0.52f, 0.34f, 0.18f, 1.0f);
    case AssetType::Material:     return ImVec4(0.46f, 0.22f, 0.22f, 1.0f);
    case AssetType::Shader:       return ImVec4(0.20f, 0.46f, 0.50f, 1.0f);
    case AssetType::Scene:        return ImVec4(0.36f, 0.36f, 0.20f, 1.0f);
    case AssetType::Folder:       return ImVec4(0.34f, 0.34f, 0.34f, 1.0f);
    default:                      return ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    }
}

const char* AssetThumbnailCache::GetBadge(AssetType type)
{
    switch (type)
    {
    case AssetType::Texture2D:    return "TEX";
    case AssetType::StaticMesh:   return "SM";
    case AssetType::SkeletalMesh: return "SKM";
    case AssetType::Animation:    return "ANIM";
    case AssetType::Material:     return "MAT";
    case AssetType::Shader:       return "SHD";
    case AssetType::Scene:        return "SCN";
    case AssetType::Folder:       return "DIR";
    default:                      return "UNK";
    }
}
