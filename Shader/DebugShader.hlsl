#include "CommonCBuffers.hlsl"
#include "CommonTexture.hlsl"
struct VSIn
{
    float3 pos : POSITION;
    float3 n   : NORMAL;
    float2 uv  : TEXCOORD0;
    float4 tangent : TANGENT;
};

struct VSOut
{
    float3 posW : POSITION;
    float3 nW   : NORMAL;
    float2 uv  : TEXCOORD0;
    float4 tangentWS : TEXCOORD3;
    float3 bitanWS   : TEXCOORD4;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    float4 pw = mul(float4(v.pos, 1), gWorld);
    o.posW = pw.xyz;

    // 디버그는 간단히 3x3 적용 + normalize
    o.nW = normalize(mul(v.n, (float3x3)gWorld));
    o.uv = v.uv;
    
    o.tangentWS = float4(mul(float4(v.tangent.xyz, 0), gWorld).xyz, v.tangent.w);
    
    float3 N = normalize(mul(v.n, (float3x3)gWorld));
    float3 T = normalize(mul(v.tangent.xyz, (float3x3)gWorld));
    float  sign = o.tangentWS.w;       

    T = normalize(T - N * dot(N, T));
    o.tangentWS = float4(T, sign);
    o.bitanWS   = normalize(cross(N, T)) * sign;
    return o;
}

struct GSOut
{
    float4 posH : SV_Position;
    float4 col  : COLOR0;
};

static void EmitNormalLine(VSOut v, inout LineStream<GSOut> outStream)
{
    float3 p0 = v.posW;
    float3 n = normalize(v.nW);
    if (dot(n, n) < 1e-6)
    {
        n = float3(0, 1, 0);
    }

    float3 p1 = p0 + n * 0.13;

    GSOut a;
    a.posH = mul(float4(p0, 1), gViewProj);
    a.col  = float4(1, 1, 0, 1);
    outStream.Append(a);

    GSOut b;
    b.posH = mul(float4(p1, 1), gViewProj);
    b.col  = float4(1, 0, 0, 1);
    outStream.Append(b);
    outStream.RestartStrip();
}

[maxvertexcount(6)]
void GSMain(triangle VSOut input[3], inout LineStream<GSOut> outStream)
{
    EmitNormalLine(input[0], outStream);
    EmitNormalLine(input[1], outStream);
    EmitNormalLine(input[2], outStream);
}

float4 PSMain(GSOut i) : SV_Target
{
    return i.col;
}
