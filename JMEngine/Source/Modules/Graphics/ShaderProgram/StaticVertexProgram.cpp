#include "StaticVertexProgram.h"
#include "ShaderCompileUtils.h"
#include "../../Game/MeshData.h"

void StaticVertexProgram::Create(ID3D11Device* device, const wchar_t* vsPath)
{
    auto vs = ShaderCompileUtils::Compile(vsPath, "VSMain", "vs_5_0");
    if (!vs) return;

    device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_VS);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(Vertex, Pos),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(Vertex, Normal),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (UINT)offsetof(Vertex, UV),      D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(Vertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    device->CreateInputLayout(
        layout, (UINT)_countof(layout),
        vs->GetBufferPointer(), vs->GetBufferSize(),
        &m_IL
    );
}
