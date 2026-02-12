#include "Renderer.h"

#include "Renderer.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <ostream>

#include "../Graphics/Dx11Context.h"
#include "../Scene/Scene.h"
#include "Cull/Frustum.h"
#include "Pass/RenderPassBase.h"
#include "SceneProxy/StaticMeshSceneProxy.h"


static inline uint64_t HashPtr(const void* p)
{
    // 포인터 기반 간단 해시(정렬키용). 엔진에서는 리소스ID 쓰는 편이 더 좋음.
    uint64_t x = reinterpret_cast<uint64_t>(p);
    // 약간 섞기
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

using namespace DirectX;

void Renderer::Create(Dx11Context& gfx)
{
    m_Frame.Create(gfx.GetDevice());
    m_ShadowPass.Create(gfx, 2048, 1024);
    m_ToneMapPass.Create(gfx);
}

// void Renderer::Render(Dx11Context& gfx, const Scene& scene, const SceneView& view)
// {
//     std::vector<DirectionalLight> dirLights;
//     std::vector<SpotLight> spotLights;
//     GatherLights(scene, dirLights, spotLights);
//
//     BuildRenderQueue(scene, m_Queue);
//
//     m_Frame.BindCommon(gfx.GetContext());
//
//     m_ShadowPass.Execute(gfx, m_Frame, view, m_Queue, dirLights, spotLights, m_ShadowOut);
//     m_ForwardPass.Execute(gfx, m_Frame, view, m_Queue, dirLights, spotLights, m_ShadowOut);
// }

void Renderer::BuildRenderQueue(Dx11Context& gfx, const Scene& scene, const SceneView& view, RenderQueue& outQueue)
{
    outQueue.Clear();

    Frustum frustum;
    frustum.Build(view.viewProj);
    int frustumCullCount = 0;

    const auto& meshes = scene.GetMesheProxies();
    outQueue.cubeMap.reserve(meshes.size());
    outQueue.opaque.reserve(meshes.size());
    outQueue.transparent.reserve(meshes.size());
    outQueue.shadowCasters.reserve(meshes.size());

    const MeshManager& MeshManager = scene.GetMeshManager();
    for (uint64_t i = 0; i < meshes.size(); ++i)
    {
        auto* proxy = meshes[i];
        // Frustum 테스트
        if (proxy->IsCubeMap() == false && frustum.Intersects(proxy->GetBounds()) == false)
        {
            frustumCullCount++;
            continue;
        }

        RenderItem it{};
        {
            it.mesh = MeshManager.GetOrCreate(gfx.GetDevice(), proxy);

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
        }
        
        if (proxy->IsCubeMap())
        {
            outQueue.cubeMap.push_back(it);
            continue;
        }

        // TODO: 투명 판정은 Material/MaterialInstance로 하기
        // 일단은 Opaque로만 보냄 (필요하면 여기서 분기)
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
            // 투명 그림자는 정책에 따라(보통 컷아웃만 그림자)
        }
    }
    // 정렬
    std::sort(outQueue.opaque.begin(), outQueue.opaque.end(),
        [](const RenderItem& a, const RenderItem& b) { return a.sortKey < b.sortKey; });

    std::sort(outQueue.transparent.begin(), outQueue.transparent.end(),
        [](const RenderItem& a, const RenderItem& b) { return a.sortKey < b.sortKey; });

    // 디버그 출력
    //std::cout << "frustumCullCount: " << frustumCullCount << std::endl;
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
    
    v.view = scene.GetCamera().GetView();
    v.proj = scene.GetCamera().GetProj();
    v.viewProj = v.view * v.proj;
    v.cameraPosition = scene.GetCamera().GetPosition();

    return v;
}

void Renderer::Render(Dx11Context& gfx, const Scene& scene)
{
    SceneView view = BuildSceneView(gfx, scene);

    std::vector<DirectionalLight> dirLights;
    std::vector<SpotLight> spotLights;
    GatherLights(scene, dirLights, spotLights);

    BuildRenderQueue(gfx, scene, view, m_Queue);

    m_Frame.UpdatePhong(gfx.GetContext(), RenderSettings::Get().LightPhong);
    m_Frame.BindCommon(gfx.GetContext());

    // 1) Shadow
    m_ShadowPass.Execute(gfx, m_Frame, view, m_Queue, dirLights, spotLights, m_ShadowOut);

    // HDR Color
    {
        ID3D11DeviceContext* ctx = gfx.GetContext();

        ID3D11RenderTargetView* rtv = gfx.GetHDRRTV();
        ID3D11DepthStencilView* dsv = gfx.GetViewBufferDSV(); // depth는 HDR 처리 필요 없음

        ctx->OMSetRenderTargets(1, &rtv, dsv);

        // Clear HDR
        float clear[4] = {0.07f, 0.07f, 0.10f, 1.0f};
        ctx->ClearRenderTargetView(rtv, clear);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        SetMainViewport(ctx, gfx.GetWidth(), gfx.GetHeight());
    }
    // 2) CubeMap
    m_CubeMapPass.Execute(gfx, m_Frame, view, m_Queue);
    
    // 3) Forward를 HDRColor에 렌더
    {
        m_ForwardPass.Execute(gfx, m_Frame, view, m_Queue, dirLights, spotLights, m_ShadowOut);
    }
    
    // 4) DebugPath
    if (RenderSettings::Get().drawNormalVector)
    {
        m_DebugPass.Execute(gfx, m_Frame, view, m_Queue);
    }

    // 5) ToneMap: HDR SRV -> BackBuffer
    {
        // ToneMapPass에 입력 SRV 넘겨주기
        m_ToneMapPass.Execute(gfx, gfx.GetHDRSRV()); 
    }
}
