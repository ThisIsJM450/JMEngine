#pragma once
#include "RenderPassBase.h"
#include "../FrameResources.h"
#include "../../Graphics/ShaderProgram.h"

class ToneMapPass : public RenderPassBase
{

public:
    void Create(Dx11Context& gfx) override;
    void Execute(Dx11Context& gfx, ID3D11ShaderResourceView* sceneColorHDR) override;
    
protected:
    void SetViewport(ID3D11DeviceContext* ctx, float w, float h);

private:
    ShaderProgram m_ShaderProgram;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_sampLinearClamp;
    
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>     m_rs;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>   m_dssNoDepth;
    Microsoft::WRL::ComPtr<ID3D11BlendState>          m_bsOpaque;
    
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbToneMap;
    CBToneMap m_cbCPU{};
};
