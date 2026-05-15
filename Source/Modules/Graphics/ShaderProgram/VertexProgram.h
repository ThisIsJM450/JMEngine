#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class VertexProgram
{
public:
    virtual ~VertexProgram() = default;
    virtual void Create(ID3D11Device* device, const wchar_t* vsPath) = 0;

    ID3D11VertexShader* GetVS() const { return m_VS.Get(); }
    ID3D11InputLayout*  GetIL() const { return m_IL.Get(); }

protected:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_IL;
};
