#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class ShaderProgram
{
public:
    ShaderProgram(){}
    ShaderProgram(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath);
    ShaderProgram(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath, const wchar_t* gsPath);
    virtual ~ShaderProgram();
    virtual void Create(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath);
    void Create(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath, const wchar_t* gsPath);

    ID3D11VertexShader* GetVS() const { return m_VS.Get(); }
    ID3D11PixelShader*  GetPS() const { return m_PS.Get(); }
    ID3D11GeometryShader*  GetGS() const { return m_GS.Get(); }
    ID3D11InputLayout*  GetIL() const { return m_IL.Get(); }

protected:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_PS;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader>  m_GS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_IL;
};

class SkinnedShaderProgram : public ShaderProgram
{
public:
    SkinnedShaderProgram() = default;
    SkinnedShaderProgram(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath);

    void Create(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath) override;
};