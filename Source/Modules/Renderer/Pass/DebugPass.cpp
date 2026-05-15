#include "DebugPass.h"

#include <DirectXMath.h>
#include "../SceneView.h"
#include "../RenderQueue.h"
#include "../FrameResources.h"
#include "../../Core/Settings/RenderSettings.h"
#include "../../Graphics/Dx11Context.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Graphics/ShaderProgram/PassVertexPrograms.h"
#include "../RenderData/GPUMesh/GPUMeshSkeletal.h"

using namespace DirectX;

static PassVertexPrograms g_DebugVP;

void DebugPass::Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& queue)
{
    ID3D11DeviceContext* context = gfx.GetContext();

    if (RenderSettings::Get().drawNormalVector)
    {
        g_DebugVP.Ensure(gfx.GetDevice(),
            L"Shader\\DebugShader.hlsl",
            L"Shader\\SkinnedDebugVS.hlsl"
        );

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

            g_DebugVP.Bind(context, it.bSkinned);
            if (it.bSkinned && it.mesh)
            {
                if (GPUMeshSkeletal* sk = static_cast<GPUMeshSkeletal*>(it.mesh))
                {
                    if (!sk->BonePalette.empty())
                    {
                        fr.UpdateBones(context, &sk->BonePalette[0], (uint32_t)sk->BonePalette.size());
                    }
                }
            }

            for (const GPUMeshSection& sec : it.mesh->Sections)
            {
                const uint32_t slot = sec.MaterialIndex;

                MaterialInstance* mi = nullptr;
                if (slot < it.materials.size() && it.materials[slot]) mi = it.materials[slot];
                else mi = gfx.GetBasicMaterialInstance();

                mi->Bind(gfx.GetDevice(), context, PassType::Debug);
                context->DrawIndexed(sec.IndexCount, sec.StartIndex, 0);
            }
        }
    }
}
