#include "LevelLoader.h"

#include "LevelAsset.h"
#include "../../Core/AppBase.h"
#include "../../Core/Asset/EngineAssetProvider.h"
#include "../../Game/World.h"
#include "../../Game/Actor.h"
#include "../../Game/Actors/CameraActor.h"
#include "../../Game/Actors/CubeMapActor.h"
#include "../../Game/Actors/DirectionalLightActor.h"
#include "../../Game/Actors/SpotLightActor.h"
#include "../../Game/Actors/StaticMeshActor.h"
#include "../Utils/MeshFactory.h"

#include <sstream>

namespace
{
    constexpr float DefaultAspectRatio = 16.0f / 9.0f;
    constexpr const char* MeshReferencePropertyKey = "asset.meshReference";
    constexpr const char* MaterialReferencePropertyKey = "asset.materialReference";
    constexpr const char* MaterialSourcePathPropertyKey = "material.sourcePath";

    bool TryParseFloat3(const std::string& text, DirectX::XMFLOAT3& out)
    {
        std::stringstream ss(text);
        char comma1 = 0;
        char comma2 = 0;
        if (ss >> out.x >> comma1 >> out.y >> comma2 >> out.z)
        {
            return comma1 == ',' && comma2 == ',';
        }
        return false;
    }

    bool TryParseFloat4(const std::string& text, DirectX::XMFLOAT4& out)
    {
        std::stringstream ss(text);
        char comma1 = 0;
        char comma2 = 0;
        char comma3 = 0;
        if (ss >> out.x >> comma1 >> out.y >> comma2 >> out.z >> comma3 >> out.w)
        {
            return comma1 == ',' && comma2 == ',' && comma3 == ',';
        }
        return false;
    }

    bool TryParseBoolInt(const std::string& text, bool& out)
    {
        if (text == "1") { out = true; return true; }
        if (text == "0") { out = false; return true; }
        return false;
    }

    const std::string* FindPropertyValue(const LevelActorDesc& actor, const char* key)
    {
        const auto it = actor.properties.find(key);
        return (it != actor.properties.end()) ? &it->second : nullptr;
    }

    std::string FindFirstReferenceProperty(const LevelActorDesc& actor, const char* primaryKey, const std::string& fallbackValue)
    {
        if (const std::string* value = FindPropertyValue(actor, primaryKey))
        {
            if (!value->empty())
            {
                return *value;
            }
        }
        return fallbackValue;
    }

    std::string CanonicalizeEngineReference(const std::string& reference, AssetType expectedType)
    {
        if (reference.empty())
        {
            return reference;
        }

        if (reference.rfind("/Engine/", 0) == 0 || reference.rfind("/Game/", 0) == 0)
        {
            return reference;
        }

        if (const EngineAssetDefinition* def = EngineAssetProvider::FindByDisplayName(reference, expectedType))
        {
            return def->virtualPath;
        }

        return reference;
    }
}

const AssetMeta* LevelLoader::ResolveAssetMetaByReference(const std::string& reference, AssetType expectedType)
{
    if (reference.empty())
    {
        return nullptr;
    }

    if (reference.rfind("/Game/", 0) != 0 && reference.rfind("/Engine/", 0) != 0)
    {
        return nullptr;
    }

    const AssetRegistry* registry = GAssetRegistry;
    if (!registry)
    {
        return nullptr;
    }

    return registry->FindByVirtualPath(reference, expectedType);
}

bool LevelLoader::LoadLevelFromFile(World& world, const std::string& path)
{
    LevelAsset level;
    if (!LevelAsset::LoadFromFile(path, level))
    {
        return false;
    }

    return ApplyLevel(world, level);
}

bool LevelLoader::ApplyLevel(World& world, const LevelAsset& level)
{
    for (const LevelActorDesc& actor : level.actors)
    {
        SpawnActorFromDescriptor(world, level, actor);
    }

    return true;
}

Actor* LevelLoader::SpawnActorFromDescriptor(World& world, const LevelAsset& level, const LevelActorDesc& actor)
{
    const std::string& actorType = actor.className.empty() ? actor.type : actor.className;

    if (actorType == "CameraActor")
    {
        return SpawnCameraActor(world, level, actor);
    }

    if (actorType == "DirectionalLightActor")
    {
        return SpawnDirectionalLightActor(world, actor);
    }

    if (actorType == "SpotLightActor")
    {
        return SpawnSpotLightActor(world, actor);
    }

    if (actorType == "StaticMeshActor")
    {
        return SpawnStaticMeshActor(world, actor);
    }

    if (actorType == "CubeMapActor")
    {
        return SpawnCubeMapActor(world, actor);
    }

    return nullptr;
}

CameraActor* LevelLoader::SpawnCameraActor(World& world, const LevelAsset& level, const LevelActorDesc& actor)
{
    auto* camera = world.SpawnActor<CameraActor>(actor.name);
    if (!camera || !camera->GetRootComponent())
    {
        return nullptr;
    }

    ApplyActorTransform(*camera, actor);

    float cameraFov = actor.cameraFovDegree;
    float cameraNearZ = actor.cameraNearZ;
    float cameraFarZ = actor.cameraFarZ;
    auto it = actor.properties.find("camera.fov");
    if (it != actor.properties.end()) cameraFov = std::stof(it->second);
    it = actor.properties.find("camera.nearZ");
    if (it != actor.properties.end()) cameraNearZ = std::stof(it->second);
    it = actor.properties.find("camera.farZ");
    if (it != actor.properties.end()) cameraFarZ = std::stof(it->second);

    camera->SetPerspective(cameraFov, DefaultAspectRatio, cameraNearZ, cameraFarZ);

    if (ShouldSetActiveCamera(level, actor))
    {
        world.SetActiveCamera(camera);
    }

    return camera;
}

Actor* LevelLoader::SpawnDirectionalLightActor(World& world, const LevelActorDesc& actor)
{
    auto* light = world.SpawnActor<DirectionalLightActor>(actor.name);
    if (!light || !light->GetRootComponent() || !light->GetLightComponent())
    {
        return nullptr;
    }

    ApplyActorTransform(*light, actor);
    DirectX::XMFLOAT3 lightColor = actor.lightColor;
    float lightIntensity = actor.lightIntensity;
    auto it = actor.properties.find("light.color");
    if (it != actor.properties.end()) TryParseFloat3(it->second, lightColor);
    it = actor.properties.find("light.intensity");
    if (it != actor.properties.end()) lightIntensity = std::stof(it->second);
    light->GetLightComponent()->color = lightColor;
    light->GetLightComponent()->intensity = lightIntensity;
    return light;
}

Actor* LevelLoader::SpawnSpotLightActor(World& world, const LevelActorDesc& actor)
{
    auto* spot = world.SpawnActor<SpotLightActor>(actor.name);
    if (!spot || !spot->GetRootComponent() || !spot->GetLightComponent())
    {
        return nullptr;
    }

    ApplyActorTransform(*spot, actor);
    DirectX::XMFLOAT3 lightColor = actor.lightColor;
    float lightIntensity = actor.lightIntensity;
    float lightRange = actor.lightRange;
    float spotAngleDegrees = actor.lightSpotAngleDegrees;
    auto it = actor.properties.find("light.color");
    if (it != actor.properties.end()) TryParseFloat3(it->second, lightColor);
    it = actor.properties.find("light.intensity");
    if (it != actor.properties.end()) lightIntensity = std::stof(it->second);
    it = actor.properties.find("light.range");
    if (it != actor.properties.end()) lightRange = std::stof(it->second);
    it = actor.properties.find("light.spotAngleDegrees");
    if (it != actor.properties.end()) spotAngleDegrees = std::stof(it->second);
    spot->GetLightComponent()->color = lightColor;
    spot->GetLightComponent()->intensity = lightIntensity;
    spot->GetLightComponent()->range = lightRange;
    spot->GetLightComponent()->spotAngleRadians = DirectX::XMConvertToRadians(spotAngleDegrees);
    return spot;
}

Actor* LevelLoader::SpawnStaticMeshActor(World& world, const LevelActorDesc& actor)
{
    auto* meshActor = world.SpawnActor<StaticMeshActor>(actor.name);
    if (!meshActor || !meshActor->GetRootComponent() || !meshActor->GetMeshComponent())
    {
        return nullptr;
    }

    ApplyActorTransform(*meshActor, actor);

    const std::string meshReference = CanonicalizeEngineReference(
        FindFirstReferenceProperty(actor, MeshReferencePropertyKey, actor.meshReference),
        AssetType::StaticMesh);
    std::shared_ptr<MeshAsset> mesh = CreateMeshFromReference(meshReference, actor.meshParam, actor.meshStacks, actor.meshSlices);
    if (mesh)
    {
        meshActor->GetMeshComponent()->SetMesh(mesh);
    }

    std::shared_ptr<MaterialInstance> material;
    const std::string materialReference = CanonicalizeEngineReference(
        FindFirstReferenceProperty(actor, MaterialReferencePropertyKey, actor.materialReference),
        AssetType::Material);
    if (const std::string* sourcePath = FindPropertyValue(actor, MaterialSourcePathPropertyKey))
    {
        if (AssetManager* assetManager = GAssetManager)
        {
            material = assetManager->LoadMaterialBySourcePath(*sourcePath);
            if (!material && !sourcePath->empty() && !materialReference.empty())
            {
                material = assetManager->LoadMaterialByVirtualPath(materialReference);
            }
        }
    }
    if (!material)
    {
        material = ResolveMaterialReference(materialReference);
    }
    if (material)
    {
        auto it = actor.properties.find("material.baseColor");
        if (it != actor.properties.end())
        {
            DirectX::XMFLOAT4 baseColor{};
            if (TryParseFloat4(it->second, baseColor))
            {
                material->SetBaseColor(baseColor);
            }
        }
        it = actor.properties.find("material.roughness");
        if (it != actor.properties.end())
        {
            material->SetRoughness(std::stof(it->second));
        }
        it = actor.properties.find("material.metallic");
        if (it != actor.properties.end())
        {
            material->SetMetallic(std::stof(it->second));
        }
        it = actor.properties.find("material.emissive");
        if (it != actor.properties.end())
        {
            DirectX::XMFLOAT3 emissive{};
            if (TryParseFloat3(it->second, emissive))
            {
                material->SetEmissive(emissive);
            }
        }
        bool boolValue = false;
        it = actor.properties.find("material.usePackedMR");
        if (it != actor.properties.end() && TryParseBoolInt(it->second, boolValue))
        {
            material->SetUsePackedMetalRough(boolValue);
        }
        it = actor.properties.find("material.useNormalMap");
        if (it != actor.properties.end() && TryParseBoolInt(it->second, boolValue))
        {
            material->SetUseNormalMap(boolValue);
        }
        it = actor.properties.find("material.useGlossMap");
        if (it != actor.properties.end() && TryParseBoolInt(it->second, boolValue))
        {
            material->SetUseGlossMap(boolValue);
        }

        for (int texIndex = 0; texIndex < 8; ++texIndex)
        {
            const std::string key = std::string("material.texture") + std::to_string(texIndex);
            it = actor.properties.find(key);
            if (it != actor.properties.end())
            {
                material->SetTexture(static_cast<uint32_t>(texIndex), it->second);
            }
        }

        meshActor->GetMeshComponent()->SetMaterial({material});
    }

    meshActor->GetMeshComponent()->MarkRenderStateDirty();
    return meshActor;
}

Actor* LevelLoader::SpawnCubeMapActor(World& world, const LevelActorDesc& actor)
{
    auto* cubeMap = world.SpawnActor<CubeMapActor>(actor.name);
    if (!cubeMap || !cubeMap->GetRootComponent())
    {
        return nullptr;
    }

    ApplyActorTransform(*cubeMap, actor);
    if (cubeMap->GetMeshComponent())
    {
        cubeMap->GetMeshComponent()->MarkRenderStateDirty();
    }
    return cubeMap;
}

void LevelLoader::ApplyActorTransform(Actor& actor, const LevelActorDesc& actorDesc)
{
    if (!actor.GetRootComponent())
    {
        return;
    }

    actor.GetRootComponent()->GetRelativeTransform().SetPosition(actorDesc.position.x, actorDesc.position.y, actorDesc.position.z);
    actor.GetRootComponent()->GetRelativeTransform().SetRotationEuler(actorDesc.rotationEuler.x, actorDesc.rotationEuler.y, actorDesc.rotationEuler.z);
    actor.GetRootComponent()->GetRelativeTransform().SetScale(actorDesc.scale.x, actorDesc.scale.y, actorDesc.scale.z);
}

bool LevelLoader::ShouldSetActiveCamera(const LevelAsset& level, const LevelActorDesc& actorDesc)
{
    if (!level.requestedActiveCameraName.empty())
    {
        return actorDesc.name == level.requestedActiveCameraName;
    }

    if (const std::string* activeValue = FindPropertyValue(actorDesc, "camera.active"))
    {
        bool bActive = false;
        if (TryParseBoolInt(*activeValue, bActive))
        {
            return bActive;
        }
    }

    return actorDesc.bSetAsActiveCamera;
}

std::shared_ptr<MeshAsset> LevelLoader::CreateMeshFromReference(const std::string& meshReference, float param, uint32_t stacks, uint32_t slices)
{
    if (const AssetMeta* meta = ResolveAssetMetaByReference(meshReference, AssetType::StaticMesh))
    {
        (void)meta;
        if (AssetManager* assetManager = GAssetManager)
        {
            if (std::shared_ptr<MeshAsset> loaded = assetManager->LoadStaticMeshByVirtualPath(meshReference))
            {
                return loaded;
            }
        }
        // Imported runtime mesh loading is not wired yet.
        // Registry lookup is now the first-class resolution step; procedural fallback remains transitional.
    }

    const std::string reference = meshReference;
    const std::string proceduralPrefix = "shape:";
    const bool bProceduralReference = (reference.rfind(proceduralPrefix, 0) == 0);
    const std::string shape = bProceduralReference
        ? reference.substr(proceduralPrefix.size())
        : reference;

    if (shape == "Sphere")
    {
        const float radius = (param > 0.0f) ? param : 1.0f;
        const uint32_t actualStacks = (stacks > 0) ? stacks : 64;
        const uint32_t actualSlices = (slices > 0) ? slices : 64;
        std::shared_ptr<MeshAsset> proceduralMesh = MeshFactory::CreateSphere(radius, actualStacks, actualSlices);
        if (proceduralMesh && bProceduralReference)
        {
            proceduralMesh->SetAssetIdentity(0, AssetType::StaticMesh, reference);
        }
        return proceduralMesh;
    }

    if (shape == "Cube")
    {
        const float half = (param > 0.0f) ? param : 1.0f;
        std::shared_ptr<MeshAsset> proceduralMesh = MeshFactory::CreateCube(half);
        if (proceduralMesh && bProceduralReference)
        {
            proceduralMesh->SetAssetIdentity(0, AssetType::StaticMesh, reference);
        }
        return proceduralMesh;
    }

    if (shape == "Plane")
    {
        const float half = (param > 0.0f) ? param : 1.0f;
        std::shared_ptr<MeshAsset> proceduralMesh = MeshFactory::CreatePlane(half);
        if (proceduralMesh && bProceduralReference)
        {
            proceduralMesh->SetAssetIdentity(0, AssetType::StaticMesh, reference);
        }
        return proceduralMesh;
    }

    return nullptr;
}

std::shared_ptr<MaterialInstance> LevelLoader::ResolveMaterialReference(const std::string& materialReference)
{
    const std::string canonicalReference = CanonicalizeEngineReference(materialReference, AssetType::Material);

    if (AssetManager* assetManager = GAssetManager)
    {
        if (std::shared_ptr<MaterialInstance> loaded = assetManager->LoadMaterialByVirtualPath(canonicalReference))
        {
            return loaded;
        }
    }

    return nullptr;
}
