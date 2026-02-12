#include "ForwardPass.h"
#include "../SceneView.h"
#include "../RenderQueue.h"
#include "../FrameResources.h"
#include "../../Graphics/Dx11Context.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Graphics/ShaderProgram/PassVertexPrograms.h"
#include <DirectXMath.h>

#include "../RenderData/GPUMesh/GPUMeshSkeletal.h"

using namespace DirectX;

static PassVertexPrograms g_ForwardVP;

void ForwardPass::Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& queue, const std::vector<DirectionalLight>& dirLights, const std::vector<SpotLight>& SpotLights, const ShadowOutput& shadows)
{
ID3D11DeviceContext* context = gfx.GetContext();

    g_ForwardVP.Ensure(gfx.GetDevice(),
       L"Shader\\ForwardLit.hlsl",
       L"Shader\\SkinnedVS.hlsl"
    );
    

    CBFrame f{};
    XMStoreFloat4x4(&f.view, XMMatrixTranspose(view.view));
    XMStoreFloat4x4(&f.Projection, XMMatrixTranspose(view.proj));
    XMStoreFloat4x4(&f.viewProjection, XMMatrixTranspose(view.viewProj));
    f.cameraPosition = view.cameraPosition;
    f.screenSize = { (float)view.width, (float)view.height };
    fr.UpdateFrame(context, f);

    CBLight light{};
    light.dirCount = (int)dirLights.size();
    light.spotCount = (int)SpotLights.size();
    for (int i = 0; i < light.dirCount && i < kMaxDirLights; ++i) light.directionalLight[i] = dirLights[i];
    for (int i = 0; i < light.spotCount && i < kMaxSpotLights; ++i) light.SpotLight[i] = SpotLights[i];
    fr.UpdateLight(context, light);

    std::vector<ID3D11ShaderResourceView*> srvs;
    srvs.reserve(shadows.directionalMaps.size() + shadows.spotMaps.size());
    for (auto& m : shadows.directionalMaps) srvs.push_back(m.GetSRV());
    for (auto& m : shadows.spotMaps) srvs.push_back(m.GetSRV());

    if (!srvs.empty())
        context->PSSetShaderResources(ParameterBlock::kMaxTextures, (UINT)srvs.size(), srvs.data());

    for (const RenderItem& it : queue.opaque)
    {
        if (!it.mesh) continue;

        ID3D11Buffer* vb = it.mesh->VB.Get();
        context->IASetVertexBuffers(0, 1, &vb, &it.mesh->Stride, &it.mesh->Offset);
        context->IASetIndexBuffer(it.mesh->IB.Get(), it.mesh->IndexFormat, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CBObject obj{};
        XMStoreFloat4x4(&obj.world, XMMatrixTranspose(it.world));
        XMStoreFloat4x4(&obj.WVP, XMMatrixTranspose(it.world * view.viewProj));
        fr.UpdateObject(context, obj);
        

        // VS/IL (Material.Bind 전에)
        g_ForwardVP.Bind(context, it.bSkinned);
        if (it.bSkinned && it.mesh)
        {
            if (GPUMeshSkeletal* Sk = static_cast<GPUMeshSkeletal*>(it.mesh))
            {
                if (Sk->BonePalette.empty() == false)
                {
                    fr.UpdateBones(context, &Sk->BonePalette[0], Sk->BonePalette.size());
                }
            }
        }

        for (const GPUMeshSection& sec : it.mesh->Sections)
        {
            const uint32_t slot = sec.MaterialIndex;

            MaterialInstance* mi = nullptr;
            if (slot < it.materials.size() && it.materials[slot]) mi = it.materials[slot];
            else mi = gfx.GetBasicMaterialInstance();

            mi->Bind(gfx.GetDevice(), context, PassType::Forward);
            context->DrawIndexed(sec.IndexCount, sec.StartIndex, 0);
        }
    }

    if (!srvs.empty())
    {
        std::vector<ID3D11ShaderResourceView*> nulls(srvs.size(), nullptr);
        context->PSSetShaderResources(ParameterBlock::kMaxTextures, (UINT)nulls.size(), nulls.data());
    }
}
