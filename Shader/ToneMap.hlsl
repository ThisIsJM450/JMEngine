Texture2D SceneColorHDR : register(t0);
SamplerState LinearClamp : register(s0);

cbuffer ToneMapCB : register(b0)
{
    float Exposure;
    float InvGamma;
    float2 _Pad;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID)
{
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 uv[3] = {
        float2(0.0, 1.0),
        float2(0.0,-1.0),
        float2(2.0, 1.0)
    };

    VSOut o;
    o.Pos = float4(pos[vid], 0.0, 1.0);
    o.UV  = uv[vid];
    return o;
}

float3 TonemapACES(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x*(a*x+b)) / (x*(c*x+d)+e));
}


float4 PSMain(VSOut i) : SV_TARGET
{
    float3 hdr = SceneColorHDR.Sample(LinearClamp, i.UV).rgb;
    hdr *= Exposure;

    float3 ldr = TonemapACES(hdr);

    // Backbuffer가 UNORM이면 감마를 여기서
    ldr = pow(ldr, InvGamma);

    return float4(ldr, 1.0);
}
