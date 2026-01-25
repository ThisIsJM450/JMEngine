#include "ForwardPass.h"
#include "../SceneView.h"
#include "../RenderQueue.h"
#include "../FrameResources.h"
#include "../../Graphics/Dx11Context.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include <DirectXMath.h>

using namespace DirectX;

static void SetMainViewport(ID3D11DeviceContext* ctx, int w, int h)
{
    D3D11_VIEWPORT vp{};
    vp.Width = (float)w;
    vp.Height = (float)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void ForwardPass::Execute(
    Dx11Context& gfx,
    FrameResources& fr,
    const SceneView& view, const RenderQueue& queue,
    const std::vector<DirectionalLight>& dirLights, const std::vector<SpotLight>& SpotLights,
    const ShadowOutput& shadows)
{
    ID3D11DeviceContext* context = gfx.GetContext();
    
    SetMainViewport(context, gfx.GetWidth(), gfx.GetHeight());

    // Frame/Light/Shadow CB 업데이트
    CBFrame f{};
    XMStoreFloat4x4(&f.viewProjection, XMMatrixTranspose(view.viewProj));
    f.cameraPosition = view.cameraPosition;
    f.screenSize = { (float)view.width, (float)view.height };
    fr.UpdateFrame(context, f);

    CBLight light{};
    light.dirCount = (int)dirLights.size();
    light.spotCount = (int)SpotLights.size();
    for (int i = 0; i < light.dirCount && i < kMaxDirLights; ++i)
    {
        light.directionalLight[i] = dirLights[i];
    }
    for (int i = 0; i < light.spotCount && i < kMaxSpotLights; ++i)
    {
        light.SpotLight[i] = SpotLights[i];
    }
    fr.UpdateLight(context, light);

    // Shadow SRV 바인딩 (예: t0~)
    // Directional: t0.., Spot: tN..
    std::vector<ID3D11ShaderResourceView*> srvs;
    srvs.reserve(shadows.directionalMaps.size() + shadows.spotMaps.size());
    for (auto& m : shadows.directionalMaps)
    {
        srvs.push_back(m.GetSRV());
    }
    for (auto& m : shadows.spotMaps)
    {
        srvs.push_back(m.GetSRV());
    }

    if (!srvs.empty())
    {
        //ParameterBlock::kMaxTextures 이후에 연결
        context->PSSetShaderResources(ParameterBlock::kMaxTextures, (UINT)srvs.size(), srvs.data());
    }

    for (const RenderItem& it : queue.opaque)
    {
        if (!it.mesh)
        {
            continue;
        }

        ID3D11Buffer* vb = it.mesh->VB.Get();
        context->IASetVertexBuffers(0, 1, &vb, &it.mesh->Stride, &it.mesh->Offset);
        context->IASetIndexBuffer(it.mesh->IB.Get(), it.mesh->IndexFormat, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CBObject obj{};
        XMStoreFloat4x4(&obj.world, XMMatrixTranspose(it.world));
        XMStoreFloat4x4(&obj.WVP, XMMatrixTranspose(it.world * view.viewProj));
        //obj.Color = it.color;
        fr.UpdateObject(context, obj);
        
        // Section 마다 나눠서 Draw 요청
        for (const GPUMeshSection& sec : it.mesh->Sections)
        {
            const uint32_t slot = sec.MaterialIndex;

            MaterialInstance* mi;
            if (slot < it.materials.size() && it.materials[slot])
            {
                mi = it.materials[slot];
            }
            else
            {
                mi = gfx.GetBasicMaterialInstance();
            }

            mi->Bind(gfx.GetDevice(), context, PassType::Forward);

            context->DrawIndexed(sec.IndexCount, sec.StartIndex, 0);
        }
        
    }

    // SRV 해제 (다음 프레임/다른 패스에서 DSV로 쓸 때 충돌 방지)
    if (!srvs.empty())
    {
        std::vector<ID3D11ShaderResourceView*> nulls(srvs.size(), nullptr);
        context->PSSetShaderResources(ParameterBlock::kMaxTextures, (UINT)nulls.size(), nulls.data());
    }
}
