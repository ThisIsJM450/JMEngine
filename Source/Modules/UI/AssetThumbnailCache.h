#pragma once

#include "../Core/Asset/AssetRegistry.h"
#include <imgui.h>
#include <wrl/client.h>
#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AssetThumbnailInfo
{
    AssetID id = 0;
    uint64_t signature = 0;
    ImVec4 tint = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    std::string badge = "UNK";
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> previewSRV;
};

class AssetThumbnailCache
{
public:
    const AssetThumbnailInfo& GetOrCreate(const AssetMeta& meta);
    void GarbageCollect(const std::vector<AssetID>& keepIds, size_t softLimit = 4096);

private:
    static uint64_t BuildSignature(const AssetMeta& meta);
    static ImVec4 GetTint(AssetType type);
    static const char* GetBadge(AssetType type);

private:
    std::unordered_map<AssetID, AssetThumbnailInfo> m_Entries;
};
