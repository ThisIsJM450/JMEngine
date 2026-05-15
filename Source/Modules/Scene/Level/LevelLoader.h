#pragma once

#include <memory>
#include <string>

#include "../../Core/Asset/AssetType.h"

struct LevelAsset;
struct LevelActorDesc;
struct AssetMeta;
class Actor;
class CameraActor;
class World;
class MaterialInstance;
class MeshAsset;
class AssetRegistry;

class LevelLoader
{
public:
    static bool LoadLevelFromFile(World& world, const std::string& path);
    static bool ApplyLevel(World& world, const LevelAsset& level);

private:
    static Actor* SpawnActorFromDescriptor(World& world, const LevelAsset& level, const LevelActorDesc& actor);
    static CameraActor* SpawnCameraActor(World& world, const LevelAsset& level, const LevelActorDesc& actor);
    static Actor* SpawnDirectionalLightActor(World& world, const LevelActorDesc& actor);
    static Actor* SpawnSpotLightActor(World& world, const LevelActorDesc& actor);
    static Actor* SpawnStaticMeshActor(World& world, const LevelActorDesc& actor);
    static Actor* SpawnCubeMapActor(World& world, const LevelActorDesc& actor);

    static void ApplyActorTransform(Actor& actor, const LevelActorDesc& actorDesc);
    static bool ShouldSetActiveCamera(const LevelAsset& level, const LevelActorDesc& actorDesc);

    static std::shared_ptr<MeshAsset> CreateMeshFromReference(const std::string& meshReference, float param, uint32_t stacks, uint32_t slices);
    static std::shared_ptr<MaterialInstance> ResolveMaterialReference(const std::string& materialReference);
    static const AssetMeta* ResolveAssetMetaByReference(const std::string& reference, AssetType expectedType);
};
