#include "Scene.h"
#include "../Game/Components/StaticMeshComponent.h"
#include "../Game/Components/DirectionalLightComponent.h"
#include "../Game/Components/SpotLightComponent.h"
#include "../Renderer/SceneProxy/StaticMeshSceneProxy.h"

std::vector<StaticMeshSceneProxy*> Scene::GetStaticMesheProxies() const
{
    std::vector<StaticMeshSceneProxy*> result;
    for (StaticMeshComponent* sm : m_StaticMeshes)
    {
        if (!sm) continue;

        StaticMeshSceneProxy* proxy = sm->GetProxy();
        if (!proxy || !proxy->Mesh ) // Material 
        {
            continue;
        }
        result.push_back(proxy);
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
