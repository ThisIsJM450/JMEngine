#include "CubeMapPass.h"
#include "../../Graphics/Dx11Context.h"
#include "../RenderQueue.h"
#include "../RenderTypes.h"
#include "../../Graphics/ShaderProgram/PassVertexPrograms.h"

static PassVertexPrograms g_CubeMapVP;

void CubeMapPass::Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& queue)
{
    ID3D11DeviceContext* context = gfx.GetContext();

    // CubeMap 전용 VS
    g_CubeMapVP.Ensure(gfx.GetDevice(),
        L"Shader\\ForwardCubeMap.hlsl",
        L"Shader\\ForwardCubeMap.hlsl"
    );

    for (const RenderItem& it : queue.cubeMap)
    {
        if (!it.mesh) continue;

        ID3D11Buffer* vb = it.mesh->VB.Get();
        context->IASetVertexBuffers(0, 1, &vb, &it.mesh->Stride, &it.mesh->Offset);
        context->IASetIndexBuffer(it.mesh->IB.Get(), it.mesh->IndexFormat, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Material.Bind 이전에 VS/IL 결정
        g_CubeMapVP.Bind(context, it.bSkinned);

        for (const GPUMeshSection& sec : it.mesh->Sections)
        {
            const uint32_t slot = sec.MaterialIndex;

            MaterialInstance* mi = nullptr;
            if (slot < it.materials.size() && it.materials[slot]) mi = it.materials[slot];
            else mi = gfx.GetBasicMaterialInstance();

            mi->Bind(gfx.GetDevice(), context, PassType::CubeMap);
            context->DrawIndexed(sec.IndexCount, sec.StartIndex, 0);
        }
    }
}
