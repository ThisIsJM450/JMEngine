#include "CommonLights.hlsl"

// 0: Lit, 1: BaseColor, 2: NormalWS, 3: Roughness, 4: Metallic
#define DEBUG_VIEW 0

cbuffer CBFrame  : register(b0) 
{ 
    float4x4 gViewProj; 
    float3 gCamPos; 
    float gPad0; 

    float2 gScreenSize; 
    float2 gPad1; 
};

cbuffer CBObject : register(b1) 
{ 
    float4x4 gWorld; 
    float4x4 gWorldViewProj; 
};

cbuffer CBLight : register(b2)
{
    DirectionalLight gDirLights[kMaxDirLights];
    SpotLight        gSpotLights[kMaxSpotLights];

    int   gDirCount;
    int   gSpotCount;
    float2 _padLightCount;
};

cbuffer CBShadow : register(b3)
{
    float4x4 gWorldToShadow[kMaxDirLights + kMaxSpotLights];
    float4   gShadowParams[kMaxDirLights + kMaxSpotLights];
};

cbuffer CBLightPhong : register(b4)
{
    float gKs;
    float gShininess;
    float gPadPhong1;
    float gPadPhong2;
};

cbuffer CBMaterial : register(b5)
{
    float4 gBaseColor;      // xyz = color, w = alpha
    float  gRoughness;
    float  gMetallic;
    float  gPadM0;
    float  gPadM1;
};