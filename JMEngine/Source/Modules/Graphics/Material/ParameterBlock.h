#pragma once

#include <array>
#include <wrl/client.h>
#include <d3d11.h>
#include <string>

#include "MaterialConstants.h"

class ParameterBlock
{
public:
    static constexpr uint32_t kMaxTextures = 8;
    static constexpr uint32_t kMaxSamplers = 8;

public:
    // ---- Scalar/Vector params ----
    void SetBaseColor(const DirectX::XMFLOAT4& c) { m_CB.BaseColor = c; m_Dirty = true; }
    void SetRoughness(float r) { m_CB.Roughness = r; m_Dirty = true; }
    void SetMetallic(float m)  { m_CB.Metallic  = m; m_Dirty = true; }
    void SetEmissive(const DirectX::XMFLOAT3& e) { m_CB.EmissiveFactor  = e; m_Dirty = true; }
    void SetUsePackedMetalRough(bool PackMetalRough) { m_CB.PackedMR_GB  = PackMetalRough; m_Dirty = true; }
    void SetUseNormalMap(bool InUseNormalMap) { m_CB.UseNormalMap  = InUseNormalMap; m_Dirty = true; }
    void SetUseGlossMap(bool InUseGlossMap) { m_CB.UseGlossMap  = InUseGlossMap; m_Dirty = true; }
    
    void EnableCubeMap(bool bEnable) { m_IsCubemap  = bEnable;}
    bool IsCubeMap() const { return m_IsCubemap; }
    
    const CBMaterial& GetCB() const { return m_CB; }

    // ---- Resources ----
    void SetTexture(uint32_t slot, const std::string& TextureFileName);
    void SetTexture(uint32_t slot, ID3D11ShaderResourceView* srv);
    void SetSampler(uint32_t slot, ID3D11SamplerState* samp);


    // ---- Bind ----
    // materialCBRegister = b4, texStartRegister = t0, sampStartRegister = s0 기본
    void Bind(ID3D11Device* dev, ID3D11DeviceContext* ctx,
              uint32_t materialCBRegister = 4,
              uint32_t texStartRegister = 0,
              uint32_t sampStartRegister = 0);


private:
    void EnsureCB(ID3D11Device* dev);
    void UpdateCBIfDirty(ID3D11DeviceContext* ctx);

    void CreateTexture(ID3D11Device* dev, const std::string filename,
                   Microsoft::WRL::ComPtr<ID3D11Texture2D> &texture,
                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> &textureResourceView);
    bool IsDDS(const std::string& filename);
    bool IsEXR(const std::string& filename);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBuffer;
    CBMaterial m_CB{};
    bool m_IsCubemap = false;
    bool m_Dirty = true;

    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, kMaxTextures> m_SRV{};
    std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, kMaxSamplers> m_Samplers{};

    // tODO: TextureAsset으로 이관
    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, kMaxTextures> m_Textures;
    std::array<std::string, kMaxTextures> m_dirtyTextureFileName;
    // ID3D11Device* m_Device = nullptr;
};
