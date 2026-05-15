#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>


class ShadowMap
{
public:
    void Create(ID3D11Device* device, uint32_t width, uint32_t height);

    ID3D11DepthStencilView* GetDSV() const { return m_DSV.Get(); }
    ID3D11ShaderResourceView* GetSRV() const { return m_SRV.Get(); }

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Tex;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DSV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
};
