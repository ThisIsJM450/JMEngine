#include "SkinnedVertexProgram.h"
#include "ShaderCompileUtils.h"
#include "../../Game/Skeletal/SkeletalMeshData.h"

void SkinnedVertexProgram::Create(ID3D11Device* device, const wchar_t* vsPath)
{
    auto vs = ShaderCompileUtils::Compile(vsPath, "VSMain", "vs_5_0");
    if (!vs) return;

    device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_VS);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(SkinnedVertex, Pos),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(SkinnedVertex, Normal),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, (UINT)offsetof(SkinnedVertex, UV),        D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, Tangent),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,      0, (UINT)offsetof(SkinnedVertex, BoneIndex), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(SkinnedVertex, BoneWeight),D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    device->CreateInputLayout(
        layout, (UINT)_countof(layout),
        vs->GetBufferPointer(), vs->GetBufferSize(),
        &m_IL
    );
}
