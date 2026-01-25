#include "CommonCBuffers.hlsl"

struct VSIn
{
    float3 pos : POSITION;
    float3 n   : NORMAL;
    float2 uv  : TEXCOORD0;
};

struct VSOut
{
    float3 posW : POSITION;
    float3 nW   : NORMAL;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    float4 pw = mul(float4(v.pos, 1), gWorld);
    o.posW = pw.xyz;

    // 디버그는 간단히 3x3 적용 + normalize
    o.nW = mul(v.n, (float3x3)gWorld);
    return o;
}

struct GSOut
{
    float4 posH : SV_Position;
    float4 col  : COLOR0;
};

[maxvertexcount(2)]
void GSMain(point VSOut input[1], inout LineStream<GSOut> outStream)
{
    float3 p0 = input[0].posW;
    float3 n  = normalize(input[0].nW);
    float3 p1 = p0 + n * 0.05;

    GSOut a;
    a.posH = mul(float4(p0, 1), gViewProj);
    a.col  = float4(0, 1, 0, 1);
    outStream.Append(a);

    GSOut b;
    b.posH = mul(float4(p1, 1), gViewProj);
    b.col  = float4(1, 0, 0, 1);
    outStream.Append(b);
}

float4 PSMain(GSOut i) : SV_Target
{
    return i.col;
}