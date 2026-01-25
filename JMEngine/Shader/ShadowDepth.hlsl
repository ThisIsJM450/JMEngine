#include "CommonCBuffers.hlsl"

struct VSIn 
{ 
    float3 pos : POSITION; 
    float3 n : NORMAL; 
    float2 uv : TEXCOORD; 
};

struct VSOut 
{ 
    float4 pos : SV_POSITION; 
};

VSOut VSMain(VSIn vin)
{
    VSOut o;
    o.pos = mul(float4(vin.pos, 1.f), gWorldViewProj);
    return o;
}

