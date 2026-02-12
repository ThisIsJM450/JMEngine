#include "Material.h"

#include "../../Renderer/RenderTypes.h"
#include "../ShaderProgram/PixelProgram.h"

void Material::UpdateForwardState(ID3D11Device* dev)
{
    RenderStateDesc rs{};
    rs.cullMode = bCullBack ? D3D11_CULL_BACK : D3D11_CULL_FRONT;
    rs.depthEnable = bDepthEnable;
    rs.depthFunc = D3D11_COMPARISON_LESS_EQUAL;
    rs.depthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    rs.blendEnable = false;
    rs.colorWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_ForwardState.Create(dev, rs);
}

void Material::UpdateShadowState(ID3D11Device* dev)
{
    RenderStateDesc rs{};
    rs.cullMode = D3D11_CULL_BACK;
    rs.depthEnable = true;
    rs.depthFunc = D3D11_COMPARISON_LESS;
    rs.depthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    rs.blendEnable = false;
    rs.colorWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_ShadowState.Create(dev, rs);
}

void Material::Bind(ID3D11DeviceContext* ctx, PassType pass)
{
    if (bDirtyState)
    {
        ID3D11Device* device = nullptr;
        ctx->GetDevice(&device);
        UpdateForwardState(device);
        UpdateShadowState(device);
        bDirtyState = false;
    }

    ID3D11Device* device = nullptr;
    ctx->GetDevice(&device);

    if (pass == PassType::Shadow)
    {
        if (!bEnableShadow) return;

        // VS/IL은 Pass가 담당
        // Shadow는 PS/GS 끔
        ctx->PSSetShader(nullptr, nullptr, 0);
        ctx->GSSetShader(nullptr, nullptr, 0);
        m_ShadowState.Bind(ctx);
        return;
    }

    if (pass == PassType::Forward || pass == PassType::CubeMap)
    {
        if (!m_ForwardPS)
        {
            m_ForwardPS = std::make_shared<PixelProgram>();
            m_ForwardPS->Create(device, psPath, gsPath);
        }

        ctx->PSSetShader(m_ForwardPS->GetPS(), nullptr, 0);
        ctx->GSSetShader(m_ForwardPS->GetGS(), nullptr, 0);
        m_ForwardState.Bind(ctx);
        return;
    }

    if (pass == PassType::Debug)
    {
        if (!m_DebugPS)
        {
            const wchar_t* DebugShaderPath = L"Shader\\DebugShader.hlsl";
            m_DebugPS = std::make_shared<PixelProgram>();
            // Debug는 PS/GS를 필요에 따라 사용 (여기선 동일 파일 사용)
            m_DebugPS->Create(device, DebugShaderPath, DebugShaderPath);
        }

        ctx->PSSetShader(m_DebugPS->GetPS(), nullptr, 0);
        ctx->GSSetShader(m_DebugPS->GetGS(), nullptr, 0);
        m_DebugState.Bind(ctx);
        return;
    }
}
