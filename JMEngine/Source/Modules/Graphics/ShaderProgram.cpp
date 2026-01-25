#include "ShaderProgram.h"
#include <d3dcompiler.h>
#include <cassert>
#include <iostream>

#include "../Game/MeshData.h"

inline Microsoft::WRL::ComPtr<ID3DBlob> Compile(
    const wchar_t* relPathFromExe,
    const char* entry,
    const char* target)
{
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    auto TryCompile = [&](const wchar_t* path) -> HRESULT
    {
        errors.Reset();
        bytecode.Reset();
        return D3DCompileFromFile(
            path,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry,
            target,
            flags,
            0,
            &bytecode,
            &errors
        );
    };

    HRESULT hr = TryCompile(relPathFromExe);
    if (FAILED(hr))
    {
        // 에러 출력
        if (errors)
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
            OutputDebugStringA("\n");
        }
        OutputDebugStringW(L"[Shader path tried] ");
        OutputDebugStringW(relPathFromExe);
        OutputDebugStringW(L"\n");
    }
    
    return bytecode;
}

ShaderProgram::ShaderProgram(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath)
{
    Create(device, vsPath, psPath);
}

ShaderProgram::ShaderProgram(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath, const wchar_t* gsPath)
{
    Create(device, vsPath, psPath, gsPath);
}

ShaderProgram::~ShaderProgram()
{
    m_VS.Reset();
    m_PS.Reset();
    m_IL.Reset();
}

void ShaderProgram::Create(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath)
{
    if (psPath != nullptr)
    {
        auto ps = Compile(psPath, "PSMain", "ps_5_0");
        device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_PS);
    }
    
    if (vsPath != nullptr)
    {
        auto vs = Compile(vsPath, "VSMain", "vs_5_0");
        device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(), nullptr, &m_VS);
        D3D11_INPUT_ELEMENT_DESC layout[] =
            {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                //TextureCoordinate는 2D이니 R32, G32
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, UV),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        device->CreateInputLayout(layout, (UINT)_countof(layout),
            vs->GetBufferPointer(), vs->GetBufferSize(), &m_IL);
    }
}

void ShaderProgram::Create(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath, const wchar_t* gsPath)
{
    if (gsPath != nullptr)
    {
        auto gs = Compile(gsPath, "GSMain", "gs_5_0");
        device->CreateGeometryShader(gs->GetBufferPointer(), gs->GetBufferSize(), nullptr, &m_GS);
    }
    Create(device, vsPath, psPath);
}

