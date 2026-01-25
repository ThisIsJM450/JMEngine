#include "ToneMapPass.h"
#include "../../Graphics/Dx11Context.h"



void ToneMapPass::Create(Dx11Context& gfx)
{
    ID3D11Device* device = gfx.GetDevice();

    // 1) Sampler
    {
        D3D11_SAMPLER_DESC samp{};
        samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.MaxLOD = D3D11_FLOAT32_MAX;
        HRESULT hr = device->CreateSamplerState(&samp, m_sampLinearClamp.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    // 2) States (추천)
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE; // fullscreen
        rd.DepthClipEnable = TRUE;
        device->CreateRasterizerState(&rd, m_rs.GetAddressOf());
    }

    {
        D3D11_DEPTH_STENCIL_DESC dd{};
        dd.DepthEnable = FALSE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        device->CreateDepthStencilState(&dd, m_dssNoDepth.GetAddressOf());
    }

    {
        D3D11_BLEND_DESC bd{};
        bd.AlphaToCoverageEnable = FALSE;
        bd.IndependentBlendEnable = FALSE;
        auto& rt0 = bd.RenderTarget[0];
        rt0.BlendEnable = FALSE;
        rt0.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd, m_bsOpaque.GetAddressOf());
    }
    
    // ToneMap ConstantBuffer (Dynamic)
    {
        D3D11_BUFFER_DESC cbd{};
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.ByteWidth = (UINT)((sizeof(CBToneMap) + 15) & ~15); // 16-byte align
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = device->CreateBuffer(&cbd, nullptr, m_cbToneMap.GetAddressOf());
        assert(SUCCEEDED(hr));
        
        m_cbCPU.Exposure = 1.0f;
        m_cbCPU.InvGamma = 1.f / 2.2f;
    }
    // Shader
    {
        const wchar_t* ShaderPath = L"Shader\\ToneMap.hlsl";
        m_ShaderProgram.Create(device, ShaderPath, ShaderPath);
    }
}

void ToneMapPass::Execute(Dx11Context& gfx, ID3D11ShaderResourceView* sceneColorHDR)
{
    ID3D11DeviceContext* ctx = gfx.GetContext();
    if (!sceneColorHDR) return;

    // 출력: 백버퍼
    gfx.BindBackbuffer(); // RTV 바인딩

    SetViewport(ctx, (float)gfx.GetWidth(), (float)gfx.GetHeight());

    // 파이프라인 상태
    ctx->RSSetState(m_rs.Get());
    ctx->OMSetDepthStencilState(m_dssNoDepth.Get(), 0);

    float blendFactor[4] = { 0,0,0,0 };
    ctx->OMSetBlendState(m_bsOpaque.Get(), blendFactor, 0xFFFFFFFF);

    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_ShaderProgram.GetVS(), nullptr, 0);
    ctx->PSSetShader(m_ShaderProgram.GetPS(), nullptr, 0);
    
    // CB update + bind 
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_cbToneMap.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &m_cbCPU, sizeof(CBToneMap));
        ctx->Unmap(m_cbToneMap.Get(), 0);
    }


    ID3D11Buffer* cb = m_cbToneMap.Get();
    ctx->PSSetConstantBuffers(0, 1, &cb);
    
    
    ctx->PSSetSamplers(0, 1, m_sampLinearClamp.GetAddressOf());

    // 입력 SRV 바인딩
    ctx->PSSetShaderResources(0, 1, &sceneColorHDR);

    // Draw fullscreen triangle
    ctx->Draw(3, 0);

    // SRV unbind 
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);
}

void ToneMapPass::SetViewport(ID3D11DeviceContext* ctx, float w, float h)
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
