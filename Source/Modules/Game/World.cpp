#include "World.h"

#include <filesystem>

#include "Actor.h"
#include "../Core/AppBase.h"
#include "../Scene/Level/LevelAsset.h"
#include "../Scene/Level/LevelLoader.h"
#include "Actors/CameraActor.h"

namespace
{
    LevelAsset BuildFallbackLevelAsset()
    {
        LevelAsset level{};
        level.name = "FallbackRuntimeLevel";
        level.requestedActiveCameraName = "DefaultCamera";

        LevelActorDesc camera{};
        camera.type = "CameraActor";
        camera.name = "DefaultCamera";
        camera.position = {0.0f, 1.3f, -3.2f};
        camera.rotationEuler = {-0.3851575f, 0.0f, 0.0f};
        camera.cameraFovDegree = 60.0f;
        camera.cameraNearZ = 0.1f;
        camera.cameraFarZ = 1000.0f;
        camera.bSetAsActiveCamera = true;
        level.actors.push_back(camera);

        LevelActorDesc spot{};
        spot.type = "SpotLightActor";
        spot.name = "SpotLight";
        spot.position = {2.0f, 2.0f, -2.0f};
        spot.lightColor = {0.5f, 0.1f, 1.0f};
        spot.lightIntensity = 50.0f;
        spot.lightRange = 20.0f;
        spot.lightSpotAngleDegrees = 360.0f;
        level.actors.push_back(spot);

        LevelActorDesc sphere1{};
        sphere1.type = "StaticMeshActor";
        sphere1.name = "Sphere1";
        sphere1.position = {0.0f, 1.0f, 0.0f};
        sphere1.meshReference = "shape:Sphere";
        sphere1.meshParam = 1.0f;
        sphere1.meshStacks = 64;
        sphere1.meshSlices = 64;
        sphere1.materialReference = "/Engine/Materials/GrassPatchyGround";
        level.actors.push_back(sphere1);

        LevelActorDesc sphere2 = sphere1;
        sphere2.name = "Sphere2";
        sphere2.position = {2.0f, 1.0f, 0.0f};
        sphere2.materialReference = "/Engine/Materials/MetalGoldPaint";
        level.actors.push_back(sphere2);

        LevelActorDesc sphere3 = sphere1;
        sphere3.name = "Sphere3";
        sphere3.position = {-2.0f, 1.0f, 0.0f};
        sphere3.materialReference.clear();
        level.actors.push_back(sphere3);

        LevelActorDesc plane{};
        plane.type = "StaticMeshActor";
        plane.name = "Plane";
        plane.position = {1.0f, 0.0f, 0.0f};
        plane.scale = {1.0f, 0.003f, 1.0f};
        plane.meshReference = "shape:Cube";
        plane.meshParam = 10.0f;
        plane.materialReference = "/Engine/Materials/White";
        level.actors.push_back(plane);

        LevelActorDesc sky{};
        sky.type = "CubeMapActor";
        sky.name = "skySphere";
        level.actors.push_back(sky);

        return level;
    }

}

void World::BeginPlay()
{
    Actor::BeginPlay();

    bool bLoadedLevel = false;
    if (AssetManager* assetManager = GAssetManager)
    {
        if (std::shared_ptr<LevelAsset> defaultLevel = assetManager->LoadLevelAssetByVirtualPath("/Game/Levels/Default"))
        {
            bLoadedLevel = LevelLoader::ApplyLevel(*this, *defaultLevel);
        }
    }
    if (!bLoadedLevel)
    {
        LevelLoader::ApplyLevel(*this, BuildFallbackLevelAsset());
    }

    if (!GetCameraManager().HasActiveCamera())
    {
        auto* camera = SpawnActor<CameraActor>(std::string("FallbackCamera"));
        camera->LookAt({ 0.0f, 1.3f, -3.2f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        camera->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        SetActiveCamera(camera);
    }

    bBeginPlay = true;
    for (auto& actor : m_Actors)
    {
        actor->BeginPlay();
    }
}

void World::EndPlay()
{
    for (auto& actor : m_Actors)
    {
        actor->EndPlay();
    }
    Actor::EndPlay();
}

void World::ClearActors()
{
    for (auto& actor : m_Actors)
    {
        if (actor)
        {
            actor->EndPlay();
        }
    }

    m_Actors.clear();
    m_CameraManager.SetActiveCamera(nullptr);
    m_Scene = Scene();
    m_WorldQuery.Clear();
}

void World::Tick(float dt)
{
    Actor::Tick(dt);
    for (std::shared_ptr<Actor>& actor : m_Actors)
    {
        actor->Tick(dt);
    }
}
