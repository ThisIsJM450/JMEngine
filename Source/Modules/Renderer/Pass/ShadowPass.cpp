#include "ShadowPass.h"
#include "../FrameResources.h"
#include "../../Graphics/Dx11Context.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include <DirectXMath.h>
#include <iostream>

#include "../../Graphics/ShaderProgram/PassVertexPrograms.h"
#include "../Cull/Frustum.h"


using namespace DirectX;


static PassVertexPrograms g_ShadowVP;

void ShadowPass::Create(Dx11Context& gfx, uint32_t dirRes, uint32_t spotRes)
{
    m_dirRes = dirRes;
    m_spotRes = spotRes;
}

static void SetViewport(ID3D11DeviceContext* ctx, float w, float h)
{
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = w;
    vp.Height = h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void ShadowPass::Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& renderQueue,
    const std::vector<DirectionalLight>& dirLights, const std::vector<SpotLight>& spotLights, ShadowOutput& output)
{
    output.directionalMaps.resize(dirLights.size());

    Frustum frustum;
    {
         frustum.Build(XMMatrixPerspectiveFovLH(XMConvertToRadians(60), 1920.f/1080.f, 0.1f, 50.f) * view.proj);
    }
    
    
    XMVECTOR center = XMVectorSet(0.f, 0.f, 0.f, 1.f);
    float radius = 10.f;

    // (3) 라이트 UpVector 오류안나도록 처리. UpVector이 LightVector과 같지 않도록처리
    auto SafeUpFromDir = [](XMVECTOR dir)
    {
        dir = XMVector3Normalize(dir);
        XMVECTOR up = XMVectorSet(0,1,0,0);
        float d = fabsf(XMVectorGetX(XMVector3Dot(dir, up)));
        return (d > 0.99f) ? XMVectorSet(0,0,1,0) : up;
    };
    
    //Directional
    for (int i = 0; i < dirLights.size(); i++)
    {
        // ShadowMap이 없으면 만들어준다.
        if (!output.directionalMaps[i].GetDSV())
        {
            output.directionalMaps[i].Create(gfx.GetDevice(), m_dirRes, m_dirRes);
        }

        const XMVECTOR lightDir = XMLoadFloat3(&dirLights[i].direction);
        const XMVECTOR up = XMLoadFloat3(&dirLights[i].up);
        const float extraBack = 2.0f;  // 튜닝 포인트
        const XMVECTOR eye = center - lightDir * (extraBack);

        const XMMATRIX LV = XMMatrixLookAtLH(eye, center, SafeUpFromDir(lightDir)); // Light방향 정의

        // (4) LV에 의해 XYZ 번위 구하기
        XMVECTOR minV = XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 1);
        XMVECTOR maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1);

        for (int c = 0; c < 8; ++c)
        {
            const DirectX::XMVECTOR& p = frustum.GetExtent(c);
            XMVECTOR pLS = XMVector3TransformCoord(p, LV);
            minV = XMVectorMin(minV, p);
            maxV = XMVectorMax(maxV, p);
        }

        float minX = XMVectorGetX(minV), minY = XMVectorGetY(minV), minZ = XMVectorGetZ(minV);
        float maxX = XMVectorGetX(maxV), maxY = XMVectorGetY(maxV), maxZ = XMVectorGetZ(maxV);

        // (5) 패딩(캐스터 잘림 방지) - 튜닝 포인트
        const float padXY = 5.0f;
        const float padZ  = 30.0f;
        minX -= padXY; maxX += padXY;
        minY -= padXY; maxY += padXY;
        minZ -= padZ;  maxZ += padZ;

        // (6) 텍셀 스냅: shimmering 완화
        {
            float width  = maxX - minX;
            float height = maxY - minY;

            float texelX = width  / m_dirRes;
            float texelY = height / m_dirRes;

            minX = floorf(minX / texelX) * texelX;
            minY = floorf(minY / texelY) * texelY;
            maxX = minX + width;
            maxY = minY + height;
        }
        

        // (7) 오프센터 오쏘
        const XMMATRIX LP = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);

        XMMATRIX lightViewProj = LV * LP;
        RenderOneShadow(gfx, fr, renderQueue, output.directionalMaps[i], lightViewProj);
        
        XMStoreFloat4x4(&dirLights[i].lightViewProj, XMMatrixTranspose(lightViewProj));
    }

    output.spotMaps.resize(spotLights.size());
    for (size_t i = 0; i < spotLights.size(); ++i)
    {
        if (!output.spotMaps[i].GetDSV())
        {
            output.spotMaps[i].Create(gfx.GetDevice(), m_spotRes, m_spotRes);
        }

        const XMVECTOR pos = XMLoadFloat3(&spotLights[i].position);
        const XMVECTOR dir = XMLoadFloat3(&spotLights[i].direction);
        const XMVECTOR at = pos + dir * 10.f;
        XMMATRIX LV = XMMatrixLookAtLH(pos, at, XMVectorSet(0,1,0,0));
        XMMATRIX LP = XMMatrixPerspectiveFovLH(spotLights[i].spotAngleRadians * 2.0f, 1.0f, 0.1f, spotLights[i].range);

        RenderOneShadow(gfx, fr, renderQueue, output.spotMaps[i], LV * LP);
    
    }
}

void ShadowPass::RenderOneShadow(Dx11Context& gfx, FrameResources& fr, const RenderQueue& renderQueue, ShadowMap& sm,
    const DirectX::XMMATRIX& lightViewProjection)
{
    ID3D11DeviceContext* context = gfx.GetContext();

    g_ShadowVP.Ensure(gfx.GetDevice(),
        L"Shader\\ShadowDepth.hlsl",
        L"Shader\\SkinnedVS.hlsl"
    );

    ID3D11RenderTargetView* nullRTV[1] = { nullptr };
    context->OMSetRenderTargets(0, nullptr, sm.GetDSV());
    context->ClearDepthStencilView(sm.GetDSV(), D3D11_CLEAR_DEPTH, 1.f, 0);
    SetViewport(context, (float)sm.GetWidth(), (float)sm.GetHeight());

    for (const RenderItem& it : renderQueue.shadowCasters)
    {
        if (!it.castShadow || !it.mesh) continue;

        ID3D11Buffer* vb = it.mesh->VB.Get();
        context->IASetVertexBuffers(0, 1, &vb, &it.mesh->Stride, &it.mesh->Offset);
        context->IASetIndexBuffer(it.mesh->IB.Get(), it.mesh->IndexFormat, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CBObject obj{};
        XMStoreFloat4x4(&obj.world, XMMatrixTranspose(it.world));
        XMStoreFloat4x4(&obj.WVP, XMMatrixTranspose(it.world * lightViewProjection));
        fr.UpdateObject(context, obj);

        // VS/IL 먼저
        g_ShadowVP.Bind(context, it.bSkinned);

        for (const GPUMeshSection& sec : it.mesh->Sections)
        {
            const uint32_t slot = sec.MaterialIndex;

            MaterialInstance* mi = nullptr;
            if (slot < it.materials.size() && it.materials[slot]) mi = it.materials[slot];
            else mi = gfx.GetBasicMaterialInstance();

            // Material은 Shadow에서 PS/GS만 끔
            mi->Bind(gfx.GetDevice(), context, PassType::Shadow);
            context->DrawIndexed(sec.IndexCount, sec.StartIndex, 0);
        }
    }

    context->OMSetRenderTargets(1, nullRTV, nullptr);
}
