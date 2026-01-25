// File: Renderer/RenderTypes.h
#pragma once
#include <DirectXMath.h>
#include <cstdint>

enum class PassType : uint8_t
{
    Shadow = 0,
    Forward = 1,
    Debug = 2,
};

enum class LightType : uint8_t
{
    Directional = 0,
    Spot = 1,
};

struct DirectionalLight
{
    mutable DirectX::XMFLOAT4X4 lightViewProj ;
    //mutable DirectX::XMFLOAT2 ShadowMapSize = DirectX::XMFLOAT2(1.f, 1.f);
    DirectX::XMFLOAT3 direction = DirectX::XMFLOAT3(0.0f, -1.0f, 0.f);
    float intensity = 1.0f;

    DirectX::XMFLOAT3 color = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    int castShadow = 1;

    DirectX::XMFLOAT3 up = DirectX::XMFLOAT3(0.0f, -1.0f, 0.f);
    float ShadowBias = 0.0005f;
    float NormalBias = 0.f;
    float _pad0[3] = {};
};
static_assert(sizeof(DirectionalLight) % 16 == 0, "CB alignment");

struct SpotLight
{
    DirectX::XMFLOAT3 position = DirectX::XMFLOAT3(0.0f, 2.0f, 0.f);
    float range = 10.0f;

    DirectX::XMFLOAT3 direction = DirectX::XMFLOAT3(0.0f, -1.0f, 0.f);
    float spotAngleRadians = DirectX::XMConvertToRadians(30.f);

    DirectX::XMFLOAT3 color = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;

    int castShadow = 1;
    float ShadowBias = 0.001f;
    float normalBias = 0.f;
    float _pad1 = 0.f;
};

struct RendererTypes
{
    
};
