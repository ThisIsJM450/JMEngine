#include "World.h"

#include "Actor.h"
#include "../Graphics/Material/Material.h"
#include "../Graphics/Material/MaterialInstance.h"
#include "../Graphics/Texture/Texture.h"
#include "../Scene/Utils/MeshFactory.h"
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
        
        auto* sun = SpawnActor<DirectionalLightActor>();
        sun->GetRootComponent()->GetRelativeTransform().SetPosition(2.f, 200, 0);
        sun->GetRootComponent()->GetRelativeTransform().SetRotationEuler(0.6f,-0.6f, 0.f);
        sun->GetLightComponent()->intensity = 1.50f;
        sun->GetLightComponent()->color = DirectX::XMFLOAT3(1.0f, 1.f, 1.f);
        
    }
    

    // Spot Light
    // auto* spot = SpawnActor<SpotLightActor>();
    // spot->GetRootComponent()->GetRelativeTransform().SetPosition(3, 5, -2);
    // spot->GetLightComponent()->range = 15.0f;

    // Cube Mesh
    auto* cube = SpawnActor<StaticMeshActor>();
    cube->GetRootComponent()->GetRelativeTransform().SetPosition(0.f, 1.f, 0.f );
    // cube->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
    cube->GetRootComponent()->GetRelativeTransform().SetRotationEuler(1.f, 2.f,  1.f);
    cube->GetMeshComponent()->SetMesh(MeshFactory::CreateSphere(1.f, 64, 64));
    //cube->GetMeshComponent()->SetMaterial(basicMat);
    // cube->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
    cube->GetMeshComponent()->MarkRenderStateDirty();

    // Sphere Mesh
    auto* sphere = SpawnActor<StaticMeshActor>();
    sphere->GetRootComponent()->GetRelativeTransform().SetPosition(2.f, 1.f, 0.f );
    // sphere->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
    sphere->GetMeshComponent()->SetMesh(MeshFactory::CreateSphere(1.f, 64, 64));
    //cube->GetMeshComponent()->SetMaterial(basicMat);
    // sphere->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
    sphere->GetMeshComponent()->MarkRenderStateDirty();
    
    {
        auto* sphere = SpawnActor<StaticMeshActor>();
        sphere->GetRootComponent()->GetRelativeTransform().SetPosition(4.f, 1.f, 0.f );
        // sphere->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
        sphere->GetMeshComponent()->SetMesh(MeshFactory::CreateSphere(1.f, 64, 64));
        //cube->GetMeshComponent()->SetMaterial(basicMat);
        // sphere->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
        sphere->GetMeshComponent()->MarkRenderStateDirty();
    }

    // 
    auto* plane = SpawnActor<StaticMeshActor>();
    plane->GetRootComponent()->GetRelativeTransform().SetPosition(1.f, 0.f, 0.f );
    plane->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 0.003f,  1.f);
    plane->GetMeshComponent()->SetMesh(MeshFactory::CreateCube(10.f));
    //cube->GetMeshComponent()->SetMaterial(basicMat);
    // plane->GetMeshComponent()->SetColor({0.f, 1.f, 0.f, 1.f});
    plane->GetMeshComponent()->MarkRenderStateDirty();
    // skySphere
    {
        auto* skySphere = SpawnActor<StaticMeshActor>();
        skySphere->GetRootComponent()->GetRelativeTransform().SetPosition(0.f, 0.f, 0.f );
        skySphere->GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
        skySphere->GetMeshComponent()->SetMesh(MeshFactory::CreateCube(100.f));
        const wchar_t* shaderPath = L"Shader\\ForwardCubeMap.hlsl";
        std::shared_ptr<Material> Mat = std::make_shared<Material>(shaderPath, shaderPath);
        if (Mat)
        {
            Mat->SetCullBack(false);
            Mat->SetEnableShadow(false);
            if (std::shared_ptr<MaterialInstance> MI = std::make_shared<MaterialInstance>(Mat))
            {
                MI->SetTexture(0, TextureFileName::GetTexture_CubeMap());
                skySphere->GetMeshComponent()->SetMaterial({MI});
            }
        }
        skySphere->GetMeshComponent()->MarkRenderStateDirty();
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
