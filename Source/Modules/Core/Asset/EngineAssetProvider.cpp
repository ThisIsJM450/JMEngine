#include "EngineAssetProvider.h"

#include <algorithm>
#include <cctype>

namespace
{
    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    const std::vector<EngineAssetDefinition> kEngineAssets =
    {
        { "/Engine/Meshes/Sphere", AssetType::StaticMesh, "", "Sphere" },
        { "/Engine/Meshes/Cube", AssetType::StaticMesh, "", "Cube" },
        { "/Engine/Meshes/Plane", AssetType::StaticMesh, "", "Plane" },
        { "/Engine/Materials/GrassPatchyGround", AssetType::Material, "", "GrassPatchyGround" },
        { "/Engine/Materials/MetalGoldPaint", AssetType::Material, "", "MetalGoldPaint" },
        { "/Engine/Materials/StoneBrick", AssetType::Material, "", "StoneBrick" },
        { "/Engine/Materials/White", AssetType::Material, "", "White" },
    };
}

const std::vector<EngineAssetDefinition>& EngineAssetProvider::GetDefinitions()
{
    return kEngineAssets;
}

const EngineAssetDefinition* EngineAssetProvider::FindByVirtualPath(const std::string& virtualPath, AssetType type)
{
    for (const EngineAssetDefinition& def : kEngineAssets)
    {
        if (virtualPath != def.virtualPath)
        {
            continue;
        }
        if (type != AssetType::Unknown && type != def.type)
        {
            continue;
        }
        return &def;
    }
    return nullptr;
}

const EngineAssetDefinition* EngineAssetProvider::FindByDisplayName(const std::string& displayName, AssetType type)
{
    if (displayName.empty())
    {
        return nullptr;
    }

    const std::string lowered = ToLower(displayName);
    for (const EngineAssetDefinition& def : kEngineAssets)
    {
        if (type != AssetType::Unknown && type != def.type)
        {
            continue;
        }

        if (lowered == ToLower(def.displayName))
        {
            return &def;
        }
    }
    return nullptr;
}
