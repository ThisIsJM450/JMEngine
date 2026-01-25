#include "DebugPass.h"

#include <DirectXMath.h>
#include "../SceneView.h"
#include "../RenderQueue.h"
#include "../FrameResources.h"
#include "../../Graphics/Dx11Context.h"
#include "../../Graphics/Material/MaterialInstance.h"

void DebugPass::Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& queue)
{
    ID3D11DeviceContext* context = gfx.GetContext();
    
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

            mi->Bind(gfx.GetDevice(), context, PassType::Debug);

            context->DrawIndexed(sec.IndexCount, sec.StartIndex, 0);
        }
    }
    context->GSSetShader(nullptr, nullptr, 0);
}
