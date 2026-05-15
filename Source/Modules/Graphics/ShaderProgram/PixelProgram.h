#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class PixelProgram
{
public:
    PixelProgram() = default;
    ~PixelProgram() = default;

    void Create(ID3D11Device* device, const wchar_t* psPath, const wchar_t* gsPath);

    ID3D11PixelShader*    GetPS() const { return m_PS.Get(); }
    ID3D11GeometryShader* GetGS() const { return m_GS.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_PS;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_GS;
};
