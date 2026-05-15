#include "RenderState.h"

#include <cassert>


static void CheckHRLocal(HRESULT hr)
{
    if (FAILED(hr)) { assert(false); abort(); }
}

void RenderState::Create(ID3D11Device* device, const RenderStateDesc& desc)
{
    // Rasterizer
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = desc.fillMode;
    rs.CullMode = desc.cullMode;
    rs.FrontCounterClockwise = desc.frontCCW;
    rs.DepthClipEnable = true;
    
    CheckHRLocal(device->CreateRasterizerState(&rs, &m_RS));
    
    rs.FillMode =  D3D11_FILL_WIREFRAME;
    CheckHRLocal(device->CreateRasterizerState(&rs, &m_RS_Wire));

    //DepthStencil
    D3D11_DEPTH_STENCIL_DESC dss{};
    dss.DepthEnable = desc.depthEnable;
    dss.DepthWriteMask = desc.depthWriteMask;
    dss.DepthFunc = desc.depthFunc;
    dss.StencilEnable = false;
    CheckHRLocal(device->CreateDepthStencilState(&dss, &m_DSS));

    // Blend
    D3D11_BLEND_DESC bs{};
    bs.AlphaToCoverageEnable = false;
    bs.IndependentBlendEnable = false;
    auto& rt = bs.RenderTarget[0];
    rt.BlendEnable = desc.blendEnable;
    rt.RenderTargetWriteMask = desc.colorWriteMask;
    
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;

    if (desc.blendEnable)
    {
        rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    }

    CheckHRLocal(device->CreateBlendState(&bs, &m_BS));
}

void RenderState::Bind(ID3D11DeviceContext* context) const
{
    context->RSSetState(RenderSettings::Get().drawAsWire ? m_RS_Wire.Get() : m_RS.Get());
    context->OMSetDepthStencilState(m_DSS.Get(), 0);

    
    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(m_BS.Get(), blendFactor, 0xffffffff);
}
