#pragma once

#include <memory>
#include "Material.h"
#include "ParameterBlock.h"

class MaterialInstance
{
public:
    explicit MaterialInstance(std::shared_ptr<Material> baseMat)
        : m_Base(std::move(baseMat)) {}

    //void Init(ID3D11Device* dev) { m_Params.Init(dev); }

    Material* GetBase() const { return m_Base.get(); }

    // 파라미터 API
    void SetBaseColor(const DirectX::XMFLOAT4& c) { m_Params.SetBaseColor(c); }
    void SetRoughness(float r) { m_Params.SetRoughness(r); }
    void SetMetallic(float m)  { m_Params.SetMetallic(m); }

    void SetTexture(uint32_t slot, const std::string& TextureFileName) {  m_Params.SetTexture(slot, TextureFileName); }
    void SetTexture(uint32_t slot, ID3D11ShaderResourceView* srv) { m_Params.SetTexture(slot, srv); }
    void SetSampler(uint32_t slot, ID3D11SamplerState* samp) { m_Params.SetSampler(slot, samp); }

    // 그리기 직전에 호출
    void Bind(ID3D11Device* Dev, ID3D11DeviceContext* Context, PassType Pass) const;

private:
    std::shared_ptr<Material> m_Base;
    mutable ParameterBlock m_Params; // Bind 시 CB 업데이트가 있어서 mutable
};
