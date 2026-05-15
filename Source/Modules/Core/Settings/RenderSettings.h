#pragma once
#include <cstdint>

struct alignas(16) CBLightPhong 
{
    float ks = 1.f;
    float shininess = 256.f;
    float Pad0 = 0.0f;
    float Pad1 = 0.0f;
};
static_assert((sizeof(CBLightPhong) % 16) == 0, "CBLightPhong must be 16-byte aligned");

struct RenderSettings
{
    static RenderSettings& Get();
    
    CBLightPhong LightPhong;
    bool drawAsWire = false;
    bool drawNormalVector = false;
};

