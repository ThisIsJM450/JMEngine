#pragma once
#include <memory>
#include <vector>

#include "Camera.h"
#include "../Game/Components/StaticMeshComponent.h"
#include "../Renderer/RenderTypes.h"
#include "Mesh/MeshManager.h"

class SpotLightComponent;
class DirectionalLightComponent;
class MeshComponent;

class Scene
{
public:
    Camera& GetCamera() { return m_Camera; }
    const Camera& GetCamera() const { return m_Camera; }

    MeshManager& GetMeshManager() { return m_MeshManager; }
    const MeshManager& GetMeshManager() const { return m_MeshManager; }

    // ---- Registration (Game thread에서 호출) ----
    void AddMesh(MeshComponent* c)
    {
        if (!c) return;
        if (std::find(m_Meshes.begin(), m_Meshes.end(), c) == m_Meshes.end())
        {
            m_Meshes.push_back(c);
            c->meshID = m_Meshes.size() - 1;
        }

    }

    void RemoveMesh(MeshComponent* c)
    {
        auto it = std::remove(m_Meshes.begin(), m_Meshes.end(), c);
        m_Meshes.erase(it, m_Meshes.end());
    }

    void AddDirectionalLight(DirectionalLightComponent* c)
    {
        if (!c) return;
        if (std::find(m_DirLights.begin(), m_DirLights.end(), c) == m_DirLights.end())
            m_DirLights.push_back(c);
    }

    void RemoveDirectionalLight(DirectionalLightComponent* c)
    {
        auto it = std::remove(m_DirLights.begin(), m_DirLights.end(), c);
        m_DirLights.erase(it, m_DirLights.end());
    }

    void AddSpotLight(SpotLightComponent* c)
    {
        if (!c) return;
        if (std::find(m_SpotLights.begin(), m_SpotLights.end(), c) == m_SpotLights.end())
            m_SpotLights.push_back(c);
    }

    void RemoveSpotLight(SpotLightComponent* c)
    {
        auto it = std::remove(m_SpotLights.begin(), m_SpotLights.end(), c);
        m_SpotLights.erase(it, m_SpotLights.end());
    }

    std::vector<StaticMeshSceneProxy*> GetStaticMesheProxies() const;
    std::vector<SceneProxyBase*> GetMesheProxies() const;
    void GetDirectionalLights(std::vector<DirectionalLight>& lights) const;
    void GetSpotLights(std::vector<SpotLight>& lights) const;

private:
    Camera m_Camera;
    MeshManager m_MeshManager;

    
    std::vector<MeshComponent*> m_Meshes;
    std::vector<DirectionalLightComponent*> m_DirLights;
    std::vector<SpotLightComponent*> m_SpotLights;
};
