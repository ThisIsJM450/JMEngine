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
    float4 pos        : SV_POSITION;
    float3 posWS      : TEXCOORD0;
    float3 normalWS   : TEXCOORD1;
    float2 uv         : TEXCOORD2;

    float4 tangentWS  : TEXCOORD3;
    float3 bitanWS    : TEXCOORD4;
};

static uint ClampBoneIndex(uint i)
{
    if (gBoneCount == 0) return 0;
    return min(i, gBoneCount - 1);
}

static float4x4 MakeSkinMatrix(uint4 idx, float4 w)
{
    // idx.x = ClampBoneIndex(idx.x);
    // idx.y = ClampBoneIndex(idx.y);
    // idx.z = ClampBoneIndex(idx.z);
    // idx.w = ClampBoneIndex(idx.w);

    return gBones[idx.x] * w.x +
           gBones[idx.y] * w.y +
           gBones[idx.z] * w.z +
           gBones[idx.w] * w.w;
}

//------------------------------------------------------
// inverse() 없이 3x3 역행렬(표준 adjugate) - HLSL용
// m의 row들이 m[0], m[1], m[2]라고 가정
//------------------------------------------------------
static float3x3 Inverse3x3(float3x3 m)
{
    float3 r0 = m[0];
    float3 r1 = m[1];
    float3 r2 = m[2];

    float3 c0 = cross(r1, r2);
    float3 c1 = cross(r2, r0);
    float3 c2 = cross(r0, r1);

    float det = dot(r0, c0);

    // det가 너무 작으면(퇴화) 안전장치: 그냥 identity
    // (이 상황 자체가 비정상 월드 행렬임)
    if (abs(det) < 1e-8f)
        return float3x3(1,0,0, 0,1,0, 0,0,1);

    // 중요: 역행렬은 transpose([c0;c1;c2]) / det
    float3x3 adjT = transpose(float3x3(c0, c1, c2));
    return adjT / det;
}

VSOut VSMain(VSIn vin)
{
    VSOut o;

    // 1) Skinning (object space)
    float4x4 skin = MakeSkinMatrix(vin.boneIdx, vin.weight);
    /*
    float3 skPos = float3(0.f, 0.f, 0.f);
    float3 skN = float3(0.f, 0.f, 0.f);
    float3 skT = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < 4; i++)
    {
        skPos += vin.weight.x * mul(float4(vin.pos, 1), gBones[vin.boneIdx.x]).xyz;
    }*/

    float3 skPos = mul(float4(vin.pos, 1), skin).xyz;
    float3 skN   = mul(vin.n, (float3x3)skin);
    float3 skT   = mul(vin.tangent.xyz, (float3x3)skin);

    float4 posW  = mul(float4(skPos,1), gWorld);
    o.pos        = mul(float4(skPos,1), gWorldViewProj);
    o.posWS      = posW.xyz;

    float3x3 world3    = (float3x3)gWorld;
    float3x3 normalMat = transpose(Inverse3x3(world3));

    float3 N = normalize(mul(skN, normalMat));
    float3 T = normalize(mul(skT, world3));
    T = normalize(T - N * dot(N,T));

    float sign = vin.tangent.w;
    o.normalWS  = N;
    o.tangentWS = float4(T, sign);
    o.bitanWS   = normalize(cross(N,T)) * sign;

    o.uv = vin.uv;
    return o;

    
}
