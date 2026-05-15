#include "AssetRegistryProvider.h"

#include <algorithm>
#include <cctype>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <unordered_map>

namespace
{
    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
        {
            return (char)std::tolower(c);
        });
        return s;
    }

    class ExtensionAssetProvider final : public IAssetRegistryProvider
    {
    public:
        const char* GetName() const override { return "ExtensionAssetProvider"; }

        bool CanHandle(const std::filesystem::path& absPath) const override
        {
            const std::string ext = ToLower(absPath.extension().string());
            return m_Map.find(ext) != m_Map.end();
        }

        bool Build(const AssetSourceInfo& source, AssetBuildResult& out) const override
        {
            const std::string ext = ToLower(source.absolutePath.extension().string());
            auto it = m_Map.find(ext);
            if (it == m_Map.end())
            {
                return false;
            }

            out.type = it->second;
            return true;
        }

    private:
        const std::unordered_map<std::string, AssetType> m_Map = {
            { ".obj",  AssetType::StaticMesh },
            { ".gltf", AssetType::StaticMesh },
            { ".png",  AssetType::Texture2D },
            { ".jpg",  AssetType::Texture2D },
            { ".jpeg", AssetType::Texture2D },
            { ".tga",  AssetType::Texture2D },
            { ".dds",  AssetType::Texture2D },
            { ".hdr",  AssetType::Texture2D },
            { ".exr",  AssetType::Texture2D },
            { ".mat",  AssetType::Material },
            { ".matasset", AssetType::Material },
            { ".mtlx", AssetType::Material },
            { ".meshasset", AssetType::StaticMesh },
            { ".hlsl", AssetType::Shader },
            { ".fx",   AssetType::Shader },
            { ".levelasset", AssetType::Level },
            { ".scene",AssetType::Scene },
            { ".json", AssetType::Scene },
        };
    };

    class FbxAssetProvider final : public IAssetRegistryProvider
    {
    public:
        const char* GetName() const override { return "FbxAssetProvider"; }

        bool CanHandle(const std::filesystem::path& absPath) const override
        {
            return ToLower(absPath.extension().string()) == ".fbx";
        }

        bool Build(const AssetSourceInfo& source, AssetBuildResult& out) const override
        {
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(source.absolutePath.string(), aiProcess_SortByPType);
            if (!scene)
            {
                return false;
            }

            if (scene->HasMeshes() && scene->mNumMeshes > 0)
            {
                bool hasSkinned = false;
                for (unsigned i = 0; i < scene->mNumMeshes; ++i)
                {
                    const aiMesh* m = scene->mMeshes[i];
                    if (m && m->HasBones() && m->mNumBones > 0)
                    {
                        hasSkinned = true;
                        break;
                    }
                }

                out.type = hasSkinned ? AssetType::SkeletalMesh : AssetType::StaticMesh;
                return true;
            }

            if (scene->HasAnimations() && scene->mNumAnimations > 0)
            {
                out.type = AssetType::Animation;
                return true;
            }

            return false;
        }
    };
}

std::vector<std::unique_ptr<IAssetRegistryProvider>> CreateDefaultAssetRegistryProviders()
{
    std::vector<std::unique_ptr<IAssetRegistryProvider>> out;
    out.emplace_back(std::make_unique<FbxAssetProvider>());
    out.emplace_back(std::make_unique<ExtensionAssetProvider>());
    return out;
}
