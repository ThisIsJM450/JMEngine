#include "Renderer.h"

#include <algorithm>

#include "Cull/Frustum.h"
#include "Pass/RenderPassBase.h"
#include "SceneProxy/StaticMeshSceneProxy.h"
#include "../Core/Settings/RenderSettings.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshSceneProxy.h"
#include "../Graphics/Dx11Context.h"
#include "../Scene/Scene.h"

using namespace DirectX;

void Renderer::Create(Dx11Context& gfx)
{
    m_Frame.Create(gfx.GetDevice());
    m_ShadowPass.Create(gfx, 2048, 1024);
    m_ToneMapPass.Create(gfx);
}

void Renderer::BuildRenderQueue(Dx11Context& gfx, const Scene& scene, const SceneView& view, RenderQueue& outQueue)
{
    outQueue.Clear();

    Frustum frustum;
    frustum.Build(view.viewProj);

    const auto& meshes = scene.GetMesheProxies();
    outQueue.cubeMap.reserve(meshes.size());
    outQueue.opaque.reserve(meshes.size());
    outQueue.transparent.reserve(meshes.size());
    outQueue.shadowCasters.reserve(meshes.size());

    const MeshManager& meshManager = scene.GetMeshManager();
    for (uint64_t i = 0; i < meshes.size(); ++i)
    {
        auto* proxy = meshes[i];
        if (proxy->IsCubeMap() == false && frustum.Intersects(proxy->GetBounds()) == false)
        {
            continue;
        }

        RenderItem it{};
        it.mesh = meshManager.GetOrCreate(gfx.GetDevice(), proxy);

        const auto& mats = proxy->GetMaterialInstances();
        if (mats.empty())
        {
            it.materials = { gfx.GetBasicMaterialInstance() };
        }
        else
        {
            it.materials = mats;
        }

        it.world = proxy->GetWorldMatrix();
        it.castShadow = proxy->GetCastShadow();
        it.receiveShadow = proxy->GetReceiveShadow();
        it.bSkinned = it.mesh->IsSkinned();
        it.bounds = proxy->GetBounds();

        if (proxy->IsCubeMap())
        {
            outQueue.cubeMap.push_back(it);
            continue;
        }

        const bool isTransparent = false;
        if (!isTransparent)
        {
            outQueue.opaque.push_back(it);
            if (it.castShadow)
            {
                outQueue.shadowCasters.push_back(it);
            }
        }
        else
        {
            outQueue.transparent.push_back(it);
        }
    }

    std::sort(outQueue.opaque.begin(), outQueue.opaque.end(),
        [](const RenderItem& a, const RenderItem& b) { return a.sortKey < b.sortKey; });

    std::sort(outQueue.transparent.begin(), outQueue.transparent.end(),
        [](const RenderItem& a, const RenderItem& b) { return a.sortKey < b.sortKey; });
}

void Renderer::GatherLights(const Scene& scene, std::vector<DirectionalLight>& outDir, std::vector<SpotLight>& outSpot)
{
    scene.GetDirectionalLights(outDir);
    scene.GetSpotLights(outSpot);
}

void Renderer::SetMainViewport(ID3D11DeviceContext* ctx, int w, int h)
{
    D3D11_VIEWPORT vp{};
    vp.Width = (float)w;
    vp.Height = (float)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

SceneView Renderer::BuildSceneView(Dx11Context& gfx, const Scene& scene)
{
    SceneView v{};
    v.width = gfx.GetWidth();
    v.height = gfx.GetHeight();

    const ViewInfo& viewInfo = scene.GetViewInfo();
    v.view = viewInfo.view;
    v.proj = viewInfo.proj;
    v.viewProj = viewInfo.viewProj;
    v.cameraPosition = viewInfo.cameraPosition;

    return v;
}

void Renderer::RenderRuntimeScene(Dx11Context& gfx, const Scene& scene)
{
    m_LastSceneView = BuildSceneView(gfx, scene);

    std::vector<DirectionalLight> dirLights;
    std::vector<SpotLight> spotLights;
    GatherLights(scene, dirLights, spotLights);

    BuildRenderQueue(gfx, scene, m_LastSceneView, m_Queue);

    m_Frame.UpdatePhong(gfx.GetContext(), RenderSettings::Get().LightPhong);
    m_Frame.BindCommon(gfx.GetContext());

    m_ShadowPass.Execute(gfx, m_Frame, m_LastSceneView, m_Queue, dirLights, spotLights, m_ShadowOut);

    {
        ID3D11DeviceContext* ctx = gfx.GetContext();

        ID3D11RenderTargetView* rtv = gfx.GetHDRRTV();
        ID3D11DepthStencilView* dsv = gfx.GetViewBufferDSV();

        ctx->OMSetRenderTargets(1, &rtv, dsv);

        float clear[4] = { 0.07f, 0.07f, 0.10f, 1.0f };
        ctx->ClearRenderTargetView(rtv, clear);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        SetMainViewport(ctx, gfx.GetWidth(), gfx.GetHeight());
    }

    m_CubeMapPass.Execute(gfx, m_Frame, m_LastSceneView, m_Queue);
    m_ForwardPass.Execute(gfx, m_Frame, m_LastSceneView, m_Queue, dirLights, spotLights, m_ShadowOut);
}

void Renderer::CompositeToBackBuffer(Dx11Context& gfx)
{
    m_ToneMapPass.Execute(gfx, gfx.GetHDRSRV());
}
