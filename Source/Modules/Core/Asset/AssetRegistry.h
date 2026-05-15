#pragma once

#include "AssetType.h"
#include "AssetRegistryProvider.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct AssetMeta
{
    AssetID id = 0;
    AssetType type = AssetType::Unknown;

    std::string virtualPath;
    std::string sourcePath;
    std::string artifactPath;

    uint64_t sourceTimestamp = 0;
    uint64_t sourceSize = 0;

    std::unordered_map<std::string, std::string> tags;
    std::vector<AssetID> dependencies;
};

struct AssetQuery
{
    std::string virtualPathPrefix;
    std::string text;
    AssetType type = AssetType::Unknown;
};

class AssetRegistry
{
public:
    AssetRegistry();

    void SetContentRoot(const std::filesystem::path& absPath);
    const std::filesystem::path& GetContentRoot() const { return m_ContentRoot; }

    void SetDatabasePath(const std::filesystem::path& absPathJson);
    const std::filesystem::path& GetDatabasePath() const { return m_DatabasePath; }

    void ScanAll();
    void ScanDirectory(const std::filesystem::path& absDir);
    bool SaveToDisk() const;
    bool LoadFromDisk();

    const AssetMeta* GetMeta(AssetID id) const;
    AssetMeta* GetMeta(AssetID id);
    const AssetMeta* FindByVirtualPath(const std::string& virtualPath, AssetType type = AssetType::Unknown) const;
    const AssetMeta* FindBySourcePath(const std::string& sourcePath, AssetType type = AssetType::Unknown) const;
    std::vector<AssetID> Query(const AssetQuery& q) const;
    std::vector<AssetID> ListDirectChildren(const std::string& parentVirtualPath, AssetType typeFilter = AssetType::Unknown) const;

    void UpsertFileFromPath(const std::filesystem::path& absSourcePath);
    void RemoveBySourcePath(const std::string& absSourcePath);
    void Clear();

    void RegisterProvider(std::unique_ptr<IAssetRegistryProvider> provider);
    void ClearProviders();
    void RegisterDefaultProviders();

private:
    void UpsertAssetMeta(const AssetMeta& meta);
    void RefreshEngineAssets();

private:
    static uint64_t ToUnixTimeSeconds(const std::filesystem::file_time_type& ft);
    std::string MakeVirtualPathFromAbsPath(const std::filesystem::path& absPath) const;
    static AssetID MakeAssetID(const std::string& virtualPath, AssetType type);

    void UpsertFileAsAsset(const std::filesystem::directory_entry& e);
    void GarbageCollectMissingFiles(const std::unordered_map<std::string, bool>& seen);
    bool BuildAssetByProviders(const AssetSourceInfo& source, AssetBuildResult& out) const;

private:
    std::filesystem::path m_ContentRoot;
    std::filesystem::path m_DatabasePath;

    std::unordered_map<AssetID, AssetMeta> m_ById;
    std::unordered_map<std::string, AssetID> m_ByVirtualPath;
    std::unordered_map<std::string, AssetID> m_BySourcePath;
    std::vector<std::unique_ptr<IAssetRegistryProvider>> m_Providers;
};
