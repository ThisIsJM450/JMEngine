#pragma once

#include "AssetType.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct AssetSourceInfo
{
    std::filesystem::path absolutePath;
    std::string sourcePath;
    std::string virtualPath;
    uint64_t sourceTimestamp = 0;
    uint64_t sourceSize = 0;
};

struct AssetBuildResult
{
    AssetType type = AssetType::Unknown;
    std::string artifactPath;
    std::unordered_map<std::string, std::string> tags;
    std::vector<AssetID> dependencies;
};

class IAssetRegistryProvider
{
public:
    virtual ~IAssetRegistryProvider() = default;

    virtual const char* GetName() const = 0;
    virtual bool CanHandle(const std::filesystem::path& absPath) const = 0;
    virtual bool Build(const AssetSourceInfo& source, AssetBuildResult& out) const = 0;
};

std::vector<std::unique_ptr<IAssetRegistryProvider>> CreateDefaultAssetRegistryProviders();

