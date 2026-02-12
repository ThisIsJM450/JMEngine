#include "World.h"

#include "Actor.h"
#include "../Graphics/Dx11Context.h"
#include "../Graphics/Material/Material.h"
#include "../Graphics/Material/MaterialInstance.h"
#include "../Graphics/Texture/Texture.h"
#include "../Scene/Utils/MeshFactory.h"
#include "Actors/CubeMapActor.h"
#include "Actors/DirectionalLightActor.h"
#include "Actors/SpotLightActor.h"
#include "Actors/StaticMeshActor.h"

void World::BeginPlay()
{
    Actor::BeginPlay();
    
    // Directional Light
    
    // {
    //     
    //     auto* sun = SpawnActor<DirectionalLightActor>();
    //     sun->GetRootComponent()->GetRelativeTransform().SetPosition(200.f, 10, 0);
    //     sun->GetRootComponent()->GetRelativeTransform().SetRotationEuler(-10, 30.f, 0);
    //     sun->GetLightComponent()->intensity = 1.500f;
    //     sun->GetLightComponent()->color = DirectX::XMFLOAT3(0.20f, 0.2f, 8.f);
    //     
    // }
    {
        
        // auto* sun = SpawnActor<DirectionalLightActor>();
        // sun->GetRootComponent()->GetRelativeTransform().SetPosition(2.f, 200, 0);
        // sun->GetRootComponent()->GetRelativeTransform().SetRotationEuler(0.6f,-0.6f, 0.f);
        // sun->GetLightComponent()->intensity = 1.50f;
        // sun->GetLightComponent()->color = DirectX::XMFLOAT3(1.0f, 1.f, 1.f);
        
    }
    

    // Spot Light
    auto* spot = SpawnActor<SpotLightActor>(std::string("SpotLight"));
    spot->GetRootComponent()->GetRelativeTransform().SetPosition(2, 2, -2);
    spot->GetLightComponent()->color = DirectX::XMFLOAT3(0.5f, 0.1f, 1.f);
    spot->GetLightComponent()->spotAngleRadians = DirectX::XMConvertToRadians(360.f);
    spot->GetLightComponent()->range = 20.0f;
    spot->GetLightComponent()->intensity = 50.0f;

    // Cube Mesh
    auto* cube = SpawnActor<StaticMeshActor>(std::string("Sphere1"));
    cube->GetRootComponent()->GetRelativeTransform().SetPosition(0.f, 1.f, 0.f );
    // cube->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
    cube->GetMeshComponent()->SetMesh(MeshFactory::CreateSphere(1.f, 64, 64));
    cube->GetMeshComponent()->SetMaterial({Dx11Context::Get().GetGrassPatchyGroundMaterialInstance()});
    // cube->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
    cube->GetMeshComponent()->MarkRenderStateDirty();

    // Sphere Mesh
    auto* sphere = SpawnActor<StaticMeshActor>(std::string("Sphere2"));
    sphere->GetRootComponent()->GetRelativeTransform().SetPosition(2.f, 1.f, 0.f );
    // sphere->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
    sphere->GetMeshComponent()->SetMesh(MeshFactory::CreateSphere(1.f, 64, 64));
    sphere->GetMeshComponent()->SetMaterial({Dx11Context::Get().GetMetalGoldPaintMaterialInstance()});
    // sphere->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
    sphere->GetMeshComponent()->MarkRenderStateDirty();
    
    {
        auto* sphere = SpawnActor<StaticMeshActor>(std::string("Sphere3"));
        sphere->GetRootComponent()->GetRelativeTransform().SetPosition(-2.f, 1.f, 0.f );
        // sphere->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
        sphere->GetMeshComponent()->SetMesh(MeshFactory::CreateSphere(1.f, 64, 64));
        //cube->GetMeshComponent()->SetMaterial(basicMat);
        // sphere->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
        sphere->GetMeshComponent()->MarkRenderStateDirty();
    }

    // 
    auto* plane = SpawnActor<StaticMeshActor>(std::string("Plane"));
    plane->GetRootComponent()->GetRelativeTransform().SetPosition(1.f, 0.f, 0.f );
    plane->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 0.003f,  1.f);
    plane->GetMeshComponent()->SetMesh(MeshFactory::CreateCube(10.f));
    plane->GetMeshComponent()->SetMaterial({Dx11Context::Get().GetWhiteMaterialInstance()});
    //cube->GetMeshComponent()->SetMaterial(basicMat);
    // plane->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
    plane->GetMeshComponent()->MarkRenderStateDirty();
    // skySphere
    {
        auto* skySphere = SpawnActor<CubeMapActor>(std::string("skySphere"));
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

void World::Tick(float dt)
{
    Actor::Tick(dt);
    // Notify Tick to actors
    for (std::shared_ptr<Actor>& actor : m_Actors)
    {
        actor->Tick(dt);
    }
}
