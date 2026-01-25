#include "ShadowMap.h"

#include <assert.h>

void ShadowMap::Create(ID3D11Device* device, uint32_t width, uint32_t height)
{
    m_Width = width;
    m_Height = height;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1; // Shadow니 Mip은 1
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R24G8_TYPELESS; //R24는 Depth, G8은 Stencil
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_Tex);
    assert(SUCCEEDED(hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsv.Texture2D.MipSlice = 0;

    hr = device->CreateDepthStencilView(m_Tex.Get(), &dsv, &m_DSV);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1; // Shadow니 Mip은 1
    srv.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(m_Tex.Get(), &srv, &m_SRV);
    assert(SUCCEEDED(hr));
}
