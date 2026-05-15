#include "AssetRegistry.h"
#include "EngineAssetProvider.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <filesystem>

#include <DirectXTex.h>
#include <DirectXTexEXR.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace
{
    struct ThumbnailFileHeader
    {
        uint32_t magic = 0x31424854; // THB1
        uint16_t width = 0;
        uint16_t height = 0;
        uint32_t rgbSize = 0;
    };

    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
        {
            return (char)std::tolower(c);
        });
        return s;
    }

    static uint64_t Fnv1a64(const void* data, size_t sz)
    {
        const uint8_t* p = (const uint8_t*)data;
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < sz; i++)
        {
            h ^= p[i];
            h *= 1099511628211ull;
        }
        return h;
    }

    static std::filesystem::path BuildTextureThumbnailCachePath(const std::filesystem::path& dbPath, const std::string& sourcePath)
    {
        const std::string key = sourcePath + "#texthumb64";
        const uint64_t h = Fnv1a64(key.data(), key.size());
        return (dbPath.parent_path() / "ThumbnailCache" / (std::to_string(h) + ".thb")).lexically_normal();
    }

    static bool SaveTextureThumbnailCache(
        const std::filesystem::path& outPath,
        const std::vector<uint8_t>& rgb,
        int w,
        int h)
    {
        if (w <= 0 || h <= 0 || rgb.empty())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);

        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs)
        {
            return false;
        }

        ThumbnailFileHeader hdr{};
        hdr.width = (uint16_t)w;
        hdr.height = (uint16_t)h;
        hdr.rgbSize = (uint32_t)rgb.size();

        ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        ofs.write(reinterpret_cast<const char*>(rgb.data()), (std::streamsize)rgb.size());
        return ofs.good();
    }

    static bool BuildTextureThumbnailRGB(
        const std::filesystem::path& sourcePath,
        std::vector<uint8_t>& outRgb,
        int& outW,
        int& outH)
    {
        outRgb.clear();
        outW = 0;
        outH = 0;

        const std::wstring wpath = sourcePath.wstring();
        const std::string ext = ToLower(sourcePath.extension().string());

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

        const DirectX::Image* src = image.GetImage(0, 0, 0);
        if (!src)
        {
            return false;
        }

        DirectX::ScratchImage decomp;
        const DirectX::Image* working = src;
        if (DirectX::IsCompressed(src->format))
        {
            hr = DirectX::Decompress(*src, DXGI_FORMAT_UNKNOWN, decomp);
            if (FAILED(hr))
            {
                return false;
            }
            working = decomp.GetImage(0, 0, 0);
            if (!working)
            {
                return false;
            }
        }

        DirectX::ScratchImage rgba;
        if (working->format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            hr = DirectX::Convert(*working, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, rgba);
            if (FAILED(hr))
            {
                return false;
            }
            working = rgba.GetImage(0, 0, 0);
            if (!working)
            {
                return false;
            }
        }

        DirectX::ScratchImage resized;
        hr = DirectX::Resize(*working, 64, 64, DirectX::TEX_FILTER_DEFAULT, resized);
        if (FAILED(hr))
        {
            return false;
        }

        const DirectX::Image* outImg = resized.GetImage(0, 0, 0);
        if (!outImg)
        {
            return false;
        }

        outW = (int)outImg->width;
        outH = (int)outImg->height;
        if (outW <= 0 || outH <= 0)
        {
            return false;
        }

        outRgb.resize((size_t)outW * (size_t)outH * 3);

        for (int y = 0; y < outH; ++y)
        {
            const uint8_t* srcRow = outImg->pixels + (size_t)y * outImg->rowPitch;
            uint8_t* dstRow = outRgb.data() + (size_t)y * (size_t)outW * 3;
            for (int x = 0; x < outW; ++x)
            {
                dstRow[x * 3 + 0] = srcRow[x * 4 + 0];
                dstRow[x * 3 + 1] = srcRow[x * 4 + 1];
                dstRow[x * 3 + 2] = srcRow[x * 4 + 2];
            }
        }

        return !outRgb.empty();
    }
}

AssetRegistry::AssetRegistry()
{
    std::filesystem::path current = std::filesystem::current_path();
    while (!current.empty())
    {
        if (std::filesystem::exists(current / "Contents") && std::filesystem::exists(current / "JMEngine.sln"))
        {
            break;
        }

        if (!current.has_parent_path() || current.parent_path() == current)
        {
            current = std::filesystem::current_path();
            break;
        }

        current = current.parent_path();
    }

    m_ContentRoot = (current / "Contents").lexically_normal();
    m_DatabasePath = (current / "AssetRegistry.json").lexically_normal();
    RegisterDefaultProviders();
    RefreshEngineAssets();
}

void AssetRegistry::SetContentRoot(const std::filesystem::path& absPath)
{
    m_ContentRoot = absPath;
}

void AssetRegistry::SetDatabasePath(const std::filesystem::path& absPathJson)
{
    m_DatabasePath = absPathJson;
}

void AssetRegistry::Clear()
{
    m_ById.clear();
    m_ByVirtualPath.clear();
    m_BySourcePath.clear();
}

void AssetRegistry::UpsertAssetMeta(const AssetMeta& meta)
{
    if (meta.id == 0 || meta.virtualPath.empty() || meta.type == AssetType::Unknown)
    {
        return;
    }

    auto itOld = m_ById.find(meta.id);
    if (itOld != m_ById.end())
    {
        const std::string prevVirtualPath = itOld->second.virtualPath;
        const std::string prevSourcePath = itOld->second.sourcePath;
        itOld->second = meta;
        if (!prevVirtualPath.empty() && prevVirtualPath != meta.virtualPath)
        {
            m_ByVirtualPath.erase(prevVirtualPath);
        }
        if (!prevSourcePath.empty() && prevSourcePath != meta.sourcePath)
        {
            m_BySourcePath.erase(prevSourcePath);
        }
    }
    else
    {
        m_ById[meta.id] = meta;
    }

    m_ByVirtualPath[meta.virtualPath] = meta.id;
    if (!meta.sourcePath.empty())
    {
        m_BySourcePath[meta.sourcePath] = meta.id;
    }
}

void AssetRegistry::RefreshEngineAssets()
{
    for (const EngineAssetDefinition& def : EngineAssetProvider::GetDefinitions())
    {
        AssetMeta meta;
        meta.id = MakeAssetID(def.virtualPath, def.type);
        meta.type = def.type;
        meta.virtualPath = def.virtualPath;
        meta.sourcePath = def.sourcePath ? def.sourcePath : "";
        meta.tags["Name"] = (def.displayName && def.displayName[0] != 0)
            ? def.displayName
            : std::filesystem::path(def.virtualPath).filename().string();
        meta.tags["Root"] = "/Engine";
        meta.tags["Provider"] = "EngineAssetProvider";
        UpsertAssetMeta(meta);
    }
}

void AssetRegistry::RegisterProvider(std::unique_ptr<IAssetRegistryProvider> provider)
{
    if (provider)
    {
        m_Providers.emplace_back(std::move(provider));
    }
}

void AssetRegistry::ClearProviders()
{
    m_Providers.clear();
}

void AssetRegistry::RegisterDefaultProviders()
{
    m_Providers = CreateDefaultAssetRegistryProviders();
}

const AssetMeta* AssetRegistry::GetMeta(AssetID id) const
{
    auto it = m_ById.find(id);
    return (it != m_ById.end()) ? &it->second : nullptr;
}

AssetMeta* AssetRegistry::GetMeta(AssetID id)
{
    auto it = m_ById.find(id);
    return (it != m_ById.end()) ? &it->second : nullptr;
}

const AssetMeta* AssetRegistry::FindByVirtualPath(const std::string& virtualPath, AssetType type) const
{
    auto it = m_ByVirtualPath.find(virtualPath);
    if (it == m_ByVirtualPath.end())
    {
        return nullptr;
    }

    const AssetMeta* meta = GetMeta(it->second);
    if (!meta)
    {
        return nullptr;
    }

    if (type != AssetType::Unknown && meta->type != type)
    {
        return nullptr;
    }

    return meta;
}

const AssetMeta* AssetRegistry::FindBySourcePath(const std::string& sourcePath, AssetType type) const
{
    auto it = m_BySourcePath.find(sourcePath);
    if (it == m_BySourcePath.end())
    {
        return nullptr;
    }

    const AssetMeta* meta = GetMeta(it->second);
    if (!meta)
    {
        return nullptr;
    }

    if (type != AssetType::Unknown && meta->type != type)
    {
        return nullptr;
    }

    return meta;
}

uint64_t AssetRegistry::ToUnixTimeSeconds(const std::filesystem::file_time_type& ft)
{
    const auto s = std::chrono::time_point_cast<std::chrono::seconds>(ft).time_since_epoch().count();
    return (uint64_t)((s < 0) ? 0 : s);
}

std::string AssetRegistry::MakeVirtualPathFromAbsPath(const std::filesystem::path& absPath) const
{
    std::filesystem::path rel;
    try
    {
        rel = std::filesystem::relative(absPath, m_ContentRoot);
    }
    catch (...)
    {
        rel = absPath.filename();
    }

    rel.replace_extension("");
    return std::string("/Game/") + rel.generic_string();
}

AssetID AssetRegistry::MakeAssetID(const std::string& virtualPath, AssetType type)
{
    const std::string key = virtualPath + "#" + std::to_string((uint32_t)type);
    return (AssetID)Fnv1a64(key.data(), key.size());
}

bool AssetRegistry::BuildAssetByProviders(const AssetSourceInfo& source, AssetBuildResult& out) const
{
    for (const std::unique_ptr<IAssetRegistryProvider>& provider : m_Providers)
    {
        if (!provider)
        {
            continue;
        }

        if (!provider->CanHandle(source.absolutePath))
        {
            continue;
        }

        AssetBuildResult candidate{};
        if (provider->Build(source, candidate) && candidate.type != AssetType::Unknown)
        {
            out = std::move(candidate);
            return true;
        }
    }

    return false;
}

void AssetRegistry::UpsertFileAsAsset(const std::filesystem::directory_entry& e)
{
    if (!e.is_regular_file())
    {
        return;
    }

    const std::filesystem::path absPath = e.path();
    const std::string sourcePath = absPath.lexically_normal().string();
    const std::string vpath = MakeVirtualPathFromAbsPath(absPath);

    uint64_t ts = 0;
    uint64_t size = 0;
    try
    {
        ts = ToUnixTimeSeconds(e.last_write_time());
        size = (uint64_t)e.file_size();
    }
    catch (...)
    {
    }

    AssetSourceInfo source{};
    source.absolutePath = absPath;
    source.sourcePath = sourcePath;
    source.virtualPath = vpath;
    source.sourceTimestamp = ts;
    source.sourceSize = size;

    AssetBuildResult built{};
    if (!BuildAssetByProviders(source, built))
    {
        return;
    }

    const AssetID id = MakeAssetID(vpath, built.type);

    // If this source file already existed under another ID (e.g. type changed),
    // remove stale mappings first so old records do not survive as ghosts.
    {
        auto itPrevBySource = m_BySourcePath.find(sourcePath);
        if (itPrevBySource != m_BySourcePath.end() && itPrevBySource->second != id)
        {
            const AssetID prevId = itPrevBySource->second;
            auto itPrevMeta = m_ById.find(prevId);
            if (itPrevMeta != m_ById.end())
            {
                m_ByVirtualPath.erase(itPrevMeta->second.virtualPath);
                m_BySourcePath.erase(itPrevMeta->second.sourcePath);
                m_ById.erase(itPrevMeta);
            }
            else
            {
                m_BySourcePath.erase(itPrevBySource);
            }
        }
    }

    AssetMeta meta;
    meta.id = id;
    meta.type = built.type;
    meta.virtualPath = vpath;
    meta.sourcePath = sourcePath;
    meta.sourceTimestamp = ts;
    meta.sourceSize = size;
    meta.artifactPath = built.artifactPath;
    meta.dependencies = built.dependencies;

    meta.tags["Ext"] = ToLower(absPath.extension().string());
    meta.tags["Name"] = absPath.stem().string();

    if (built.type == AssetType::Texture2D)
    {
        bool keepExistingThumb = false;
        auto itExisting = m_ById.find(id);
        if (itExisting != m_ById.end())
        {
            const AssetMeta& old = itExisting->second;
            const auto itOldThumbPath = old.tags.find("ThumbnailCachePath");
            const auto itOldThumbW = old.tags.find("ThumbnailW");
            const auto itOldThumbH = old.tags.find("ThumbnailH");
            if (old.sourceTimestamp == ts && old.sourceSize == size &&
                itOldThumbPath != old.tags.end() && !itOldThumbPath->second.empty() &&
                itOldThumbW != old.tags.end() && !itOldThumbW->second.empty() &&
                itOldThumbH != old.tags.end() && !itOldThumbH->second.empty() &&
                std::filesystem::exists(itOldThumbPath->second))
            {
                meta.tags["ThumbnailCachePath"] = itOldThumbPath->second;
                meta.tags["ThumbnailW"] = itOldThumbW->second;
                meta.tags["ThumbnailH"] = itOldThumbH->second;
                keepExistingThumb = true;
            }
        }

        if (!keepExistingThumb)
        {
            std::vector<uint8_t> thumbRgb;
            int thumbW = 0;
            int thumbH = 0;
            if (BuildTextureThumbnailRGB(absPath, thumbRgb, thumbW, thumbH))
            {
                const std::filesystem::path cachePath = BuildTextureThumbnailCachePath(m_DatabasePath, sourcePath);
                if (SaveTextureThumbnailCache(cachePath, thumbRgb, thumbW, thumbH))
                {
                    meta.tags["ThumbnailCachePath"] = cachePath.string();
                    meta.tags["ThumbnailW"] = std::to_string(thumbW);
                    meta.tags["ThumbnailH"] = std::to_string(thumbH);
                }
            }
        }
    }
    for (const auto& kv : built.tags)
    {
        meta.tags[kv.first] = kv.second;
    }

    auto itOld = m_ById.find(id);
    if (itOld != m_ById.end())
    {
        AssetMeta& old = itOld->second;
        const std::string prevVirtualPath = old.virtualPath;
        const std::string prevSourcePath = old.sourcePath;
        old.type = meta.type;
        old.virtualPath = meta.virtualPath;
        old.sourcePath = meta.sourcePath;
        old.sourceTimestamp = meta.sourceTimestamp;
        old.sourceSize = meta.sourceSize;
        old.artifactPath = meta.artifactPath;
        old.dependencies = meta.dependencies;
        old.tags = std::move(meta.tags);

        if (prevVirtualPath != old.virtualPath)
        {
            m_ByVirtualPath.erase(prevVirtualPath);
        }
        if (prevSourcePath != old.sourcePath)
        {
            m_BySourcePath.erase(prevSourcePath);
        }
        m_ByVirtualPath[old.virtualPath] = id;
        m_BySourcePath[old.sourcePath] = id;
        return;
    }

    m_ById[id] = meta;
    m_ByVirtualPath[vpath] = id;
    m_BySourcePath[sourcePath] = id;
}

void AssetRegistry::GarbageCollectMissingFiles(const std::unordered_map<std::string, bool>& seen)
{
    std::vector<AssetID> toRemove;
    toRemove.reserve(m_ById.size());

    for (auto& kv : m_ById)
    {
        const AssetMeta& m = kv.second;
        if (seen.find(m.sourcePath) == seen.end())
        {
            toRemove.push_back(m.id);
        }
    }

    for (AssetID id : toRemove)
    {
        auto it = m_ById.find(id);
        if (it == m_ById.end())
        {
            continue;
        }

        m_ByVirtualPath.erase(it->second.virtualPath);
        m_BySourcePath.erase(it->second.sourcePath);
        m_ById.erase(it);
    }
}

void AssetRegistry::ScanDirectory(const std::filesystem::path& absDir)
{
    if (!std::filesystem::exists(absDir))
    {
        return;
    }

    for (auto& e : std::filesystem::recursive_directory_iterator(absDir))
    {
        if (!e.is_regular_file())
        {
            continue;
        }
        UpsertFileAsAsset(e);
    }
}

void AssetRegistry::ScanAll()
{
    if (!std::filesystem::exists(m_ContentRoot))
    {
        return;
    }

    std::unordered_map<std::string, bool> seen;
    for (auto& e : std::filesystem::recursive_directory_iterator(m_ContentRoot))
    {
        if (!e.is_regular_file())
        {
            continue;
        }

        const std::string sp = e.path().lexically_normal().string();
        seen[sp] = true;
        UpsertFileAsAsset(e);
    }

    GarbageCollectMissingFiles(seen);
    RefreshEngineAssets();
}

std::vector<AssetID> AssetRegistry::Query(const AssetQuery& q) const
{
    std::vector<AssetID> out;
    out.reserve(m_ById.size());

    const std::string text = ToLower(q.text);
    const std::string prefix = q.virtualPathPrefix;

    for (const auto& kv : m_ById)
    {
        const AssetMeta& m = kv.second;

        if (q.type != AssetType::Unknown && m.type != q.type)
        {
            continue;
        }

        if (!prefix.empty() && m.virtualPath.rfind(prefix, 0) != 0)
        {
            continue;
        }

        if (!text.empty())
        {
            const std::string vp = ToLower(m.virtualPath);
            const std::string name = ToLower(m.tags.count("Name") ? m.tags.at("Name") : "");
            if (vp.find(text) == std::string::npos && name.find(text) == std::string::npos)
            {
                continue;
            }
        }

        out.push_back(m.id);
    }

    std::sort(out.begin(), out.end(), [&](AssetID a, AssetID b)
    {
        const AssetMeta* A = GetMeta(a);
        const AssetMeta* B = GetMeta(b);
        if (!A || !B)
        {
            return a < b;
        }
        return A->virtualPath < B->virtualPath;
    });

    return out;
}

std::vector<AssetID> AssetRegistry::ListDirectChildren(const std::string& parentVirtualPath, AssetType typeFilter) const
{
    std::vector<AssetID> out;
    out.reserve(256);

    std::string prefix = parentVirtualPath;
    if (!prefix.empty() && prefix.back() != '/')
    {
        prefix.push_back('/');
    }

    for (const auto& kv : m_ById)
    {
        const AssetMeta& m = kv.second;

        if (typeFilter != AssetType::Unknown && m.type != typeFilter)
        {
            continue;
        }

        if (m.virtualPath.rfind(prefix, 0) != 0)
        {
            continue;
        }

        const std::string rest = m.virtualPath.substr(prefix.size());
        if (rest.find('/') != std::string::npos)
        {
            continue;
        }

        out.push_back(m.id);
    }

    std::sort(out.begin(), out.end(), [&](AssetID a, AssetID b)
    {
        const AssetMeta* A = GetMeta(a);
        const AssetMeta* B = GetMeta(b);
        if (!A || !B)
        {
            return a < b;
        }
        return A->virtualPath < B->virtualPath;
    });

    return out;
}

void AssetRegistry::UpsertFileFromPath(const std::filesystem::path& absSourcePath)
{
    std::error_code ec;
    if (!std::filesystem::exists(absSourcePath, ec) || ec)
    {
        return;
    }

    std::filesystem::directory_entry e(absSourcePath, ec);
    if (ec || !e.is_regular_file())
    {
        return;
    }

    UpsertFileAsAsset(e);
}

void AssetRegistry::RemoveBySourcePath(const std::string& absSourcePath)
{
    auto itId = m_BySourcePath.find(absSourcePath);
    if (itId == m_BySourcePath.end())
    {
        return;
    }

    const AssetID id = itId->second;
    auto it = m_ById.find(id);
    if (it == m_ById.end())
    {
        return;
    }

    m_ByVirtualPath.erase(it->second.virtualPath);
    m_BySourcePath.erase(it->second.sourcePath);
    m_ById.erase(it);
}

bool AssetRegistry::SaveToDisk() const
{
    try
    {
        json j;
        j["contentRoot"] = m_ContentRoot.string();
        j["assets"] = json::array();

        for (const auto& kv : m_ById)
        {
            const AssetMeta& m = kv.second;
            json a;
            a["id"] = m.id;
            a["type"] = std::string(AssetTypeToString(m.type));
            a["virtualPath"] = m.virtualPath;
            a["sourcePath"] = m.sourcePath;
            a["artifactPath"] = m.artifactPath;
            a["sourceTimestamp"] = m.sourceTimestamp;
            a["sourceSize"] = m.sourceSize;

            json tags = json::object();
            for (const auto& t : m.tags)
            {
                tags[t.first] = t.second;
            }
            a["tags"] = tags;

            json deps = json::array();
            for (AssetID d : m.dependencies)
            {
                deps.push_back(d);
            }
            a["dependencies"] = deps;

            j["assets"].push_back(a);
        }

        std::ofstream ofs(m_DatabasePath, std::ios::binary);
        if (!ofs)
        {
            return false;
        }

        ofs << j.dump(2);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool AssetRegistry::LoadFromDisk()
{
    try
    {
        std::ifstream ifs(m_DatabasePath, std::ios::binary);
        if (!ifs)
        {
            return false;
        }

        json j;
        ifs >> j;

        Clear();

        if (j.contains("contentRoot"))
        {
            m_ContentRoot = std::filesystem::path(j["contentRoot"].get<std::string>());
        }

        if (!j.contains("assets"))
        {
            return true;
        }

        for (auto& a : j["assets"])
        {
            AssetMeta m;
            m.id = a.value("id", 0ull);
            m.type = AssetTypeFromString(a.value("type", "Unknown"));
            m.virtualPath = a.value("virtualPath", "");
            m.sourcePath = a.value("sourcePath", "");
            m.artifactPath = a.value("artifactPath", "");
            m.sourceTimestamp = a.value("sourceTimestamp", 0ull);
            m.sourceSize = a.value("sourceSize", 0ull);

            if (a.contains("tags"))
            {
                for (auto it = a["tags"].begin(); it != a["tags"].end(); ++it)
                {
                    m.tags[it.key()] = it.value().get<std::string>();
                }
            }

            if (a.contains("dependencies"))
            {
                for (auto& d : a["dependencies"])
                {
                    m.dependencies.push_back(d.get<uint64_t>());
                }
            }

            m_ById[m.id] = m;
            if (!m.virtualPath.empty())
            {
                m_ByVirtualPath[m.virtualPath] = m.id;
            }
            if (!m.sourcePath.empty())
            {
                m_BySourcePath[m.sourcePath] = m.id;
            }
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}
