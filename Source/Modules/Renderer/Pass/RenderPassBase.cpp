#include "RenderPassBase.h"

void RenderPassBase::BeginPass(ID3D11DeviceContext* ctx, const PassBeginDesc& desc)
{
    // 1) OM 바인딩
    ID3D11RenderTargetView* rtv = desc.rtv;
    ctx->OMSetRenderTargets(1, &rtv, desc.dsv);

    // 2) Viewport
    if (desc.setViewport)
    {
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width  = desc.viewportW;
        vp.Height = desc.viewportH;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
    }

    // 3) Clear
    if (desc.clearColor && desc.rtv)
    {
        ctx->ClearRenderTargetView(desc.rtv, desc.clearRGBA);
    }

    if (desc.clearDepth && desc.dsv)
    {
        UINT flags = D3D11_CLEAR_DEPTH;
        if (desc.clearStencil) flags |= D3D11_CLEAR_STENCIL;

        ctx->ClearDepthStencilView(desc.dsv, flags, desc.clearDepthValue, desc.clearStencilValue);
    }
}