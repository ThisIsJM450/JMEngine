#include "CommonLights.hlsl"

// 0: Lit, 1: BaseColor, 2: NormalWS, 3: Roughness, 4: Metallic
#define DEBUG_VIEW 0

cbuffer CBFrame  : register(b0) 
{ 
    float4x4 gView; 
    float4x4 gProj; 
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

cbuffer MaterialCB : register(b5)
{
    float4 gBaseColorFactor;   // (1,1,1,1) 기본
    float  gRoughnessFactor;   // 기본 1
    float  gMetallicFactor;    // 기본 0
    float  gNormalScale;       // 기본 1 (원하면)
    float  gAOFactor;          // 기본 1
    float3 gEmissiveFactor;    // 기본 0

    uint   gPackedMR_GB;       // 1이면 t2에서 G=Rough, B=Metal / 0이면 분리 metallic로 보고 R=Metal
    uint   gUseNormalMap;      // 1이면 normal map 사용
    uint   gUseGlossMap;      // 1이면 Gloss map 사용
    uint   gMaterialCB_Pad0;
};