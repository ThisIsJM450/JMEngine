#include "Scene.h"
#include "../Game/Components/DirectionalLightComponent.h"
#include "../Game/Components/SpotLightComponent.h"
#include "../Renderer/SceneProxy/StaticMeshSceneProxy.h"

std::vector<StaticMeshSceneProxy*> Scene::GetStaticMesheProxies() const
{
    std::vector<StaticMeshSceneProxy*> result;
    for (auto sm : m_Meshes)
    {
        if (!sm) continue;

        StaticMeshSceneProxy* proxy = dynamic_cast<StaticMeshSceneProxy*>(sm->GetProxy());
        if (!proxy) 
        {
            continue;
        }
        result.push_back(proxy);
    }
    return result;
}

std::vector<SceneProxyBase*> Scene::GetMesheProxies() const
{
    std::vector<SceneProxyBase*> result;
    for (MeshComponent* sm : m_Meshes)
    {
        if (!sm) continue;
        
        result.push_back(sm->GetProxy());
    }
    return result;
}

void Scene::GetDirectionalLights(std::vector<DirectionalLight>& lights) const
{
    lights.clear();
    for (DirectionalLightComponent* d : m_DirLights)
    {
        if (d)
        {
            lights.push_back(d->GetLightData());
        }
    }
}

void Scene::GetSpotLights(std::vector<SpotLight>& lights) const
{
    lights.clear();
    for (SpotLightComponent* d : m_SpotLights)
    {
        if (d)
        {
            lights.push_back(d->GetLightData());
        }
    }
}
