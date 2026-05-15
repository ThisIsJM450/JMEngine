#include "AssetManager.h"

#include "EngineAssetProvider.h"

#include "../../Graphics/Importer/FBXImporter.h"
#include "../../Graphics/Importer/FBXImporter_Animation.h"
#include "../../Game/Skeletal/SkeletalMeshData.h"
#include "../../Game/MeshData.h"
#include "../../Graphics/Dx11Context.h"
#include "../../Graphics/Material/Material.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Scene/Level/LevelAsset.h"
#include "../../Scene/Utils/MeshFactory.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    std::string ToLower(std::string value)
    {
        for (char& c : value)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file)
        {
            return {};
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string ExtractQuotedValue(const std::string& text, size_t startPos, const char* key)
    {
        const std::string token = std::string(key) + "=\"";
        const size_t tokenPos = text.find(token, startPos);
        if (tokenPos == std::string::npos)
        {
            return {};
        }

        const size_t valueBegin = tokenPos + token.size();
        const size_t valueEnd = text.find('"', valueBegin);
        if (valueEnd == std::string::npos)
        {
            return {};
        }

        return text.substr(valueBegin, valueEnd - valueBegin);
    }

    std::string FindMaterialXPathInputValue(const std::string& document, const char* nodeName)
    {
        const std::string nodeToken = std::string("name=\"") + nodeName + "\"";
        const size_t nodePos = document.find(nodeToken);
        if (nodePos == std::string::npos)
        {
            return {};
        }

        const size_t fileInputPos = document.find("name=\"file\"", nodePos);
        if (fileInputPos == std::string::npos)
        {
            return {};
        }

        return ExtractQuotedValue(document, fileInputPos, "value");
    }

    std::filesystem::path ResolveSiblingSourcePath(const std::filesystem::path& sourcePath, const std::string& relativeOrAbsolutePath)
    {
        if (relativeOrAbsolutePath.empty())
        {
            return {};
        }

        std::filesystem::path path(relativeOrAbsolutePath);
        if (path.is_absolute())
        {
            return path.lexically_normal();
        }

        return (sourcePath.parent_path() / path).lexically_normal();
    }

    void ApplyTextureIfExists(MaterialInstance& instance, uint32_t slot, const std::filesystem::path& path)
    {
        if (path.empty() || !std::filesystem::exists(path))
        {
            return;
        }

        instance.SetTexture(slot, path.string());
    }

    void ApplyKnownMaterialFolderDefaults(MaterialInstance& instance, const std::string& sourcePathLower)
    {
        if (sourcePathLower.find("grasspatchyground") != std::string::npos)
        {
            instance.SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            instance.SetRoughness(1.0f);
            instance.SetMetallic(0.0f);
            return;
        }

        if (sourcePathLower.find("metalgoldpaint") != std::string::npos)
        {
            instance.SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            instance.SetRoughness(0.3f);
            instance.SetMetallic(1.0f);
            return;
        }

        if (sourcePathLower.find("stonebrick") != std::string::npos || sourcePathLower.find("stonebricks") != std::string::npos)
        {
            instance.SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            instance.SetRoughness(0.5f);
            instance.SetMetallic(0.0f);
        }
    }

    std::shared_ptr<MaterialInstance> BuildMaterialInstanceFromMtlx(
        const std::string& sourcePath,
        const AssetMeta* meta)
    {
        const std::filesystem::path mtlxPath(sourcePath);
        if (!std::filesystem::exists(mtlxPath))
        {
            return nullptr;
        }

        const std::string document = ReadTextFile(mtlxPath);
        if (document.empty())
        {
            return nullptr;
        }

        auto base = Dx11Context::Get().GetSharedBasicMaterial();
        if (!base)
        {
            return nullptr;
        }

        auto instance = std::make_shared<MaterialInstance>(base);
        instance->SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        instance->SetRoughness(1.0f);
        instance->SetMetallic(0.0f);

        const std::filesystem::path baseColorPath = ResolveSiblingSourcePath(mtlxPath, FindMaterialXPathInputValue(document, "BaseColor_Map"));
        const std::filesystem::path normalPath = ResolveSiblingSourcePath(mtlxPath, FindMaterialXPathInputValue(document, "Normal_Map"));
        const std::filesystem::path metallicPath = ResolveSiblingSourcePath(mtlxPath, FindMaterialXPathInputValue(document, "Metallic_Map"));
        const std::filesystem::path roughnessPath = ResolveSiblingSourcePath(mtlxPath, FindMaterialXPathInputValue(document, "Roughness_Map"));
        const std::filesystem::path aoPath = ResolveSiblingSourcePath(mtlxPath, FindMaterialXPathInputValue(document, "AmbientOcclusion_Map"));
        const std::filesystem::path emissivePath = ResolveSiblingSourcePath(mtlxPath, FindMaterialXPathInputValue(document, "Emissive_Map"));

        ApplyTextureIfExists(*instance, 0, baseColorPath);
        ApplyTextureIfExists(*instance, 1, normalPath);
        ApplyTextureIfExists(*instance, 2, metallicPath);
        ApplyTextureIfExists(*instance, 3, aoPath);
        ApplyTextureIfExists(*instance, 4, emissivePath);
        ApplyTextureIfExists(*instance, 5, roughnessPath);

        instance->SetUseNormalMap(!normalPath.empty() && std::filesystem::exists(normalPath));
        instance->SetUsePackedMetalRough(false);
        instance->SetUseGlossMap(false);

        ApplyKnownMaterialFolderDefaults(*instance, ToLower(sourcePath));

        if (meta)
        {
            instance->SetAssetIdentity(meta->id, meta->type, meta->virtualPath, meta->sourcePath);
        }
        else
        {
            const std::string virtualPath = std::string("/Game/Materials/") + mtlxPath.stem().string();
            instance->SetAssetIdentity(0, AssetType::Material, virtualPath, sourcePath);
        }

        return instance;
    }

    std::shared_ptr<MaterialInstance> TryBuildMaterialInstanceFromSource(
        const AssetMeta* meta,
        const std::string& sourcePath)
    {
        if (sourcePath.empty())
        {
            return nullptr;
        }

        const std::filesystem::path path(sourcePath);
        const std::string ext = ToLower(path.extension().string());
        if (ext == ".mtlx")
        {
            return BuildMaterialInstanceFromMtlx(sourcePath, meta);
        }

        return nullptr;
    }

    std::shared_ptr<MeshAsset> LoadBuiltInMeshByDefinition(const EngineAssetDefinition& def, const AssetMeta& meta)
    {
        std::shared_ptr<MeshAsset> mesh;
        const std::string displayName = ToLower(def.displayName ? def.displayName : "");
        if (displayName == "sphere")
        {
            mesh = MeshFactory::CreateSphere(1.0f, 64, 64);
        }
        else if (displayName == "cube")
        {
            mesh = MeshFactory::CreateCube(1.0f);
        }
        else if (displayName == "plane")
        {
            mesh = MeshFactory::CreatePlane(1.0f);
        }

        if (mesh)
        {
            mesh->SetAssetIdentity(meta.id, meta.type, meta.virtualPath, meta.sourcePath);
        }
        return mesh;
    }

    std::shared_ptr<MaterialInstance> LoadBuiltInMaterialByDefinition(const EngineAssetDefinition& def, const AssetMeta& meta)
    {
        const Dx11Context& gfx = Dx11Context::Get();
        std::shared_ptr<MaterialInstance> mat;
        const std::string displayName = ToLower(def.displayName ? def.displayName : "");
        if (displayName == "grasspatchyground")
        {
            mat = gfx.GetGrassPatchyGroundMaterialInstance();
        }
        else if (displayName == "metalgoldpaint")
        {
            mat = gfx.GetMetalGoldPaintMaterialInstance();
        }
        else if (displayName == "stonebrick")
        {
            mat = gfx.GetStoneBrickMaterialInstance();
        }
        else if (displayName == "white")
        {
            mat = gfx.GetWhiteMaterialInstance();
        }

        if (mat)
        {
            mat->SetAssetIdentity(meta.id, meta.type, meta.virtualPath, meta.sourcePath);
        }
        return mat;
    }
}

AssetManager::AssetManager(const AssetRegistry* registry)
    : m_Registry(registry)
{
}

void AssetManager::SetRegistry(const AssetRegistry* registry)
{
    m_Registry = registry;
}

void AssetManager::Clear()
{
    m_AnimCache.clear();
}

std::shared_ptr<AnimSequenceAsset> AssetManager::LoadAnimSequence(AssetID id, const Skeleton& skeleton)
{
    if (!m_Registry || id == 0)
    {
        return nullptr;
    }

    AnimCacheKey key{};
    key.id = id;
    key.skeleton = &skeleton;

    const auto it = m_AnimCache.find(key);
    if (it != m_AnimCache.end())
    {
        if (std::shared_ptr<AnimSequenceAsset> cached = it->second.lock())
        {
            return cached;
        }
    }

    const AssetMeta* meta = m_Registry->GetMeta(id);
    if (!meta || meta->type != AssetType::Animation)
    {
        return nullptr;
    }

    std::shared_ptr<AnimSequenceAsset> loaded =
        FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(meta->sourcePath, skeleton);
    if (loaded)
    {
        m_AnimCache[key] = loaded;
    }
    return loaded;
}

std::shared_ptr<LevelAsset> AssetManager::LoadLevelAsset(AssetID id)
{
    if (!m_Registry || id == 0)
    {
        return nullptr;
    }

    const AssetMeta* meta = m_Registry->GetMeta(id);
    if (!meta || meta->type != AssetType::Level)
    {
        return nullptr;
    }

    auto asset = std::make_shared<LevelAsset>();
    if (!LevelAsset::LoadFromFile(meta->sourcePath, *asset))
    {
        return nullptr;
    }
    return asset;
}

std::shared_ptr<LevelAsset> AssetManager::LoadLevelAssetByVirtualPath(const std::string& virtualPath)
{
    if (!m_Registry || virtualPath.empty())
    {
        return nullptr;
    }

    const AssetMeta* meta = m_Registry->FindByVirtualPath(virtualPath, AssetType::Level);
    if (!meta)
    {
        return nullptr;
    }

    return LoadLevelAsset(meta->id);
}

std::shared_ptr<MeshAsset> AssetManager::LoadStaticMeshByVirtualPath(const std::string& virtualPath)
{
    if (!m_Registry || virtualPath.empty())
    {
        return nullptr;
    }

    const AssetMeta* meta = m_Registry->FindByVirtualPath(virtualPath, AssetType::StaticMesh);
    if (!meta)
    {
        return nullptr;
    }

    if (const EngineAssetDefinition* engineDef = EngineAssetProvider::FindByVirtualPath(virtualPath, AssetType::StaticMesh))
    {
        if (std::shared_ptr<MeshAsset> builtInMesh = LoadBuiltInMeshByDefinition(*engineDef, *meta))
        {
            return builtInMesh;
        }
    }

    if (!meta->sourcePath.empty())
    {
        ImportOptions options{};
        ImportResult result{};
        if (FbxImporter::ImportFBX(meta->sourcePath, options, result) && result.StaticMesh)
        {
            result.StaticMesh->SetAssetIdentity(meta->id, meta->type, meta->virtualPath, meta->sourcePath);
            return result.StaticMesh;
        }
    }

    return nullptr;
}

std::shared_ptr<SkeletalMeshAsset> AssetManager::LoadSkeletalMeshByVirtualPath(const std::string& virtualPath)
{
    if (!m_Registry || virtualPath.empty())
    {
        return nullptr;
    }

    const AssetMeta* meta = m_Registry->FindByVirtualPath(virtualPath, AssetType::SkeletalMesh);
    if (!meta || meta->sourcePath.empty())
    {
        return nullptr;
    }

    ImportOptions options{};
    ImportResult result{};
    if (FbxImporter::ImportFBX(meta->sourcePath, options, result) && result.SkeletalMesh)
    {
        result.SkeletalMesh->SetAssetIdentity(meta->id, meta->type, meta->virtualPath, meta->sourcePath);
        return result.SkeletalMesh;
    }

    return nullptr;
}

std::shared_ptr<MaterialInstance> AssetManager::LoadMaterialByVirtualPath(const std::string& virtualPath)
{
    if (!m_Registry || virtualPath.empty())
    {
        return nullptr;
    }

    const AssetMeta* meta = m_Registry->FindByVirtualPath(virtualPath, AssetType::Material);
    if (!meta)
    {
        return nullptr;
    }

    if (std::shared_ptr<MaterialInstance> rebuilt = TryBuildMaterialInstanceFromSource(meta, meta->sourcePath))
    {
        return rebuilt;
    }

    if (const EngineAssetDefinition* engineDef = EngineAssetProvider::FindByVirtualPath(virtualPath, AssetType::Material))
    {
        if (std::shared_ptr<MaterialInstance> builtInMaterial = LoadBuiltInMaterialByDefinition(*engineDef, *meta))
        {
            return builtInMaterial;
        }
    }

    if (!meta->sourcePath.empty())
    {
        auto base = std::make_shared<Material>();
        auto instance = std::make_shared<MaterialInstance>(base);
        instance->SetAssetIdentity(meta->id, meta->type, meta->virtualPath, meta->sourcePath);
        return instance;
    }

    return nullptr;
}

std::shared_ptr<MaterialInstance> AssetManager::LoadMaterialBySourcePath(const std::string& sourcePath)
{
    if (sourcePath.empty())
    {
        return nullptr;
    }

    const AssetMeta* meta = nullptr;
    if (m_Registry)
    {
        meta = m_Registry->FindBySourcePath(sourcePath, AssetType::Material);
    }

    if (std::shared_ptr<MaterialInstance> rebuilt = TryBuildMaterialInstanceFromSource(meta, sourcePath))
    {
        return rebuilt;
    }

    if (meta && !meta->virtualPath.empty())
    {
        return LoadMaterialByVirtualPath(meta->virtualPath);
    }

    return nullptr;
}
