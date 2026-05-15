#include "CommonCBuffers.hlsl"
StructuredBuffer<float4x4> gBones : register(t0);

cbuffer CBBoneMeta : register(b6)
{
    uint gBoneCount;
    uint3 _padBoneMeta;
};

struct VSIn
{
    float3 pos     : POSITION;
    float3 n       : NORMAL;
    float2 uv      : TEXCOORD;
    float4 tangent : TANGENT;

    uint4  boneIdx : BLENDINDICES;
    float4 weight  : BLENDWEIGHT;
};

struct VSOut
{
    float3 posW      : POSITION;
    float3 nW        : NORMAL;
    float2 uv        : TEXCOORD0;
    float4 tangentWS : TEXCOORD3;
    float3 bitanWS   : TEXCOORD4;
};

static float4x4 MakeSkinMatrix(uint4 idx, float4 w)
{
    return gBones[idx.x] * w.x +
           gBones[idx.y] * w.y +
           gBones[idx.z] * w.z +
           gBones[idx.w] * w.w;
}

VSOut VSMain(VSIn vin)
{
    VSOut o;

    float4x4 skin = MakeSkinMatrix(vin.boneIdx, vin.weight);

    float3 skPos = mul(float4(vin.pos, 1), skin).xyz;
    float3 skN   = mul(vin.n, (float3x3)skin);
    float3 skT   = mul(vin.tangent.xyz, (float3x3)skin);

    float4 posW = mul(float4(skPos, 1), gWorld);
    o.posW = posW.xyz;

    float3 N = normalize(mul(skN, (float3x3)gWorld));
    float3 T = normalize(mul(skT, (float3x3)gWorld));
    T = normalize(T - N * dot(N, T));

    float sign = vin.tangent.w;

    o.nW = N;
    o.uv = vin.uv;
    o.tangentWS = float4(T, sign);
    o.bitanWS = normalize(cross(N, T)) * sign;
    return o;
}
