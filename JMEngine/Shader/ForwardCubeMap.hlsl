#include "CommonCBuffers.hlsl"
#include "CommonTexture.hlsl"
SamplerState CubeMapSampler : register(s2);

struct VSIn 
{ 
    float3 pos : POSITION; 
    float3 n : NORMAL; 
    float2 uv : TEXCOORD; 
};

struct VSOut
{
    float4 posH    : SV_Position;
    float3 worldPos: TEXCOORD0;  // 추가
};

VSOut VSMain(VSIn vin)
{
    VSOut o;

    // 큐브를 카메라 중심에 고정
    float3 worldPos = vin.pos + gCamPos;

    o.worldPos = worldPos;
    o.posH = mul(float4(worldPos, 1.0f), gViewProj);

    return o;
}

static const float PI = 3.14159265359f;

float2 DirToLatLongUV(float3 dir)
{
    dir = normalize(dir);

    float u = 0.5f + atan2(dir.z, dir.x) / (2.0f * PI);
    float v = 0.5f - asin(clamp(dir.y, -1.0f, 1.0f)) / PI;

    return float2(frac(u), saturate(v));
}

float4 PSMain(VSOut pin) : SV_Target
{
    // 카메라 기준 방향 (픽셀 단위로 확정)
    float3 dir = normalize(pin.worldPos - gCamPos);
    float2 uv = DirToLatLongUV(dir);
    return ParamTexture[0].Sample(CubeMapSampler, uv);
}