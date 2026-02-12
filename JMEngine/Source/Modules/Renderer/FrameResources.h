#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>

#include "RenderTypes.h"
#include "../Core/Settings/RenderSettings.h"

struct alignas(16) CBFrame
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 Projection;
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT3 cameraPosition;
    float _pad0;

    DirectX::XMFLOAT2 screenSize;
    DirectX::XMFLOAT2 _pad1;
};

struct alignas(16) CBObject
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 WVP;

    //DirectX::XMFLOAT4 Color;
};

static constexpr int kMaxDirLights = 2;
static constexpr int kMaxSpotLights = 8;

struct alignas(16) CBLight
{
    DirectionalLight directionalLight[kMaxDirLights];
    SpotLight SpotLight[kMaxSpotLights];

    int dirCount = 0;
    int spotCount = 0;
    DirectX::XMFLOAT2 _pad0 = {};
};

// Shadow matrices: “라이트별 World->Shadow(텍스처 공간)”
// Directional: 0..DirCount-1
// Spot: offset = kMaxDirLights, 0..SpotCount-1
struct alignas(16) CBShadow
{
    DirectX::XMFLOAT4X4 worldToShadow[kMaxDirLights + kMaxSpotLights];
    DirectX::XMFLOAT4 ShadowParams[kMaxDirLights + kMaxSpotLights];
};

struct alignas(16) CBToneMap
{
    float Exposure;
    float InvGamma;
    float Pad[2];
};

struct alignas(16) CBBoneMeta
{
    uint32_t BoneCount = 0;
    uint32_t _pad[3] = { 0,0,0 };
};

class FrameResources
{
public:
    void Create(ID3D11Device* device);
    void BindCommon(ID3D11DeviceContext* context);
    
    void UpdateFrame(ID3D11DeviceContext* ctx, const CBFrame& data);
    void UpdateObject(ID3D11DeviceContext* ctx, const CBObject& data);
    void UpdateLight(ID3D11DeviceContext* ctx, const CBLight& data);
    void UpdateShadow(ID3D11DeviceContext* ctx, const CBShadow& data);
    void UpdatePhong(ID3D11DeviceContext* ctx, const CBLightPhong& data);
    void UpdateBones(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4X4* bones, uint32_t boneCount);

    ID3D11SamplerState* GetCommonSampler() const { return m_CommonSampler.Get(); }
    ID3D11SamplerState* GetShadowSampler() const { return m_ShadowSampler.Get(); }
    ID3D11SamplerState* GetCubeMapSampler() const { return m_CubeMapSampler.Get(); }



private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBFrame;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBObject;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBLight;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBShadow;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBLightPhong;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_CommonSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_ShadowSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_CubeMapSampler;
    
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_BoneSB;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_BoneSRV;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBBoneMeta;

};

