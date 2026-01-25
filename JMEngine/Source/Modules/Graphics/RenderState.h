#pragma once
#include <d3d11.h>
#include <wrl/client.h>

#include "../Core/Settings/RenderSettings.h"

struct RenderStateDesc
{
    // Rasterizer
    D3D11_CULL_MODE cullMode = D3D11_CULL_BACK;
    D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID;

    bool frontCCW = false;

    //Depth
    bool depthEnable = true;
    D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS_EQUAL;
    D3D11_DEPTH_WRITE_MASK depthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    //Blend
    bool blendEnable = false;
    D3D11_COLOR_WRITE_ENABLE colorWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
};

class RenderState
{
public:
    void Create(ID3D11Device* device, const RenderStateDesc& desc);
    void Bind(ID3D11DeviceContext* context) const;

private:
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_RS;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_RS_Wire;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DSS;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_BS;    
};
