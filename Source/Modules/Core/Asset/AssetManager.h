#pragma once

#include <memory>
#include <unordered_map>

#include "AssetRegistry.h"

struct Skeleton;
struct AnimSequenceAsset;
struct LevelAsset;
class MeshAsset;
class Material;
class MaterialInstance;
class SkeletalMeshAsset;

class AssetManager
{
public:
    explicit AssetManager(const AssetRegistry* registry = nullptr);

    void SetRegistry(const AssetRegistry* registry);
    void Clear();

    std::shared_ptr<AnimSequenceAsset> LoadAnimSequence(AssetID id, const Skeleton& skeleton);
    std::shared_ptr<LevelAsset> LoadLevelAsset(AssetID id);
    std::shared_ptr<LevelAsset> LoadLevelAssetByVirtualPath(const std::string& virtualPath);
    std::shared_ptr<MeshAsset> LoadStaticMeshByVirtualPath(const std::string& virtualPath);
    std::shared_ptr<SkeletalMeshAsset> LoadSkeletalMeshByVirtualPath(const std::string& virtualPath);
    std::shared_ptr<MaterialInstance> LoadMaterialByVirtualPath(const std::string& virtualPath);
    std::shared_ptr<MaterialInstance> LoadMaterialBySourcePath(const std::string& sourcePath);

private:
    struct AnimCacheKey
    {
        AssetID id = 0;
        const Skeleton* skeleton = nullptr;

        bool operator==(const AnimCacheKey& rhs) const
        {
            return id == rhs.id && skeleton == rhs.skeleton;
        }
    };

    struct AnimCacheKeyHash
    {
        size_t operator()(const AnimCacheKey& k) const
        {
            const size_t h1 = std::hash<uint64_t>{}(k.id);
            const size_t h2 = std::hash<const void*>{}(k.skeleton);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

private:
    const AssetRegistry* m_Registry = nullptr;
    std::unordered_map<AnimCacheKey, std::weak_ptr<AnimSequenceAsset>, AnimCacheKeyHash> m_AnimCache;
};

