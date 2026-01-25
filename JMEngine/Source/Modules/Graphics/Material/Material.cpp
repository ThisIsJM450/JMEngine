#include "Material.h"

#include "../../Core/Utils/Utils.h"
#include "../../Renderer/RenderTypes.h"

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
        ID3D11Device* device;
        ctx->GetDevice(&device);
        UpdateForwardState(device);
        UpdateShadowState(device);
        bDirtyState = false;
    }
    
    if (pass == PassType::Shadow)
    {
        if (bEnableShadow)
        {
            if (m_Shadow == nullptr)
            {
                ID3D11Device* device;
                ctx->GetDevice(&device);
                m_Shadow = std::make_shared<ShaderProgram>(device, vsPath_Shadow, nullptr);
            }
            if (m_Shadow)
            {
                ctx->IASetInputLayout(m_Shadow->GetIL());
                ctx->VSSetShader(m_Shadow->GetVS(), nullptr, 0);
                ctx->PSSetShader(nullptr, nullptr, 0); // 이전 PS 제거
                m_ShadowState.Bind(ctx);
            }
        }
        return;
    }
    else if (pass == PassType::Forward)
    {
        if (m_Forward == nullptr)
        {
            ID3D11Device* device;
            ctx->GetDevice(&device);
            m_Forward = std::make_shared<ShaderProgram>(device, vsPath, psPath, gsPath);
        }
        if (m_Forward)
        {
            ctx->IASetInputLayout(m_Forward->GetIL());
            ctx->VSSetShader(m_Forward->GetVS(), nullptr, 0);
            ctx->PSSetShader(m_Forward->GetPS(), nullptr, 0);
            ctx->GSSetShader(m_Forward->GetGS(), nullptr, 0);
            m_ForwardState.Bind(ctx);
        }
    }
    else if (pass == PassType::Debug)
    {
        if (m_Debug == nullptr)
        {
            ID3D11Device* device;
            ctx->GetDevice(&device);
            const wchar_t* DebugShaderPath = L"Shader\\DebugShader.hlsl";
            m_Debug = std::make_shared<ShaderProgram>(device, DebugShaderPath, DebugShaderPath, DebugShaderPath);
        }
        if (m_Debug)
        {
            ctx->IASetInputLayout(m_Forward->GetIL()); // Forward꺼 그대로 쓰기..
            ctx->VSSetShader(m_Debug->GetVS(), nullptr, 0);
            ctx->PSSetShader(m_Debug->GetPS(), nullptr, 0);
            ctx->GSSetShader(m_Debug->GetGS(), nullptr, 0);
            m_DebugState.Bind(ctx);
        }
    }
}
