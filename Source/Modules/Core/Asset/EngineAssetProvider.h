#pragma once

#include "AssetType.h"

#include <string>
#include <vector>

struct EngineAssetDefinition
{
    const char* virtualPath = "";
    AssetType type = AssetType::Unknown;
    const char* sourcePath = "";
    const char* displayName = "";
};

class EngineAssetProvider
{
public:
    static const std::vector<EngineAssetDefinition>& GetDefinitions();
    static const EngineAssetDefinition* FindByVirtualPath(const std::string& virtualPath, AssetType type = AssetType::Unknown);
    static const EngineAssetDefinition* FindByDisplayName(const std::string& displayName, AssetType type = AssetType::Unknown);
};
