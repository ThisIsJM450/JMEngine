#include "CommonCBuffers.hlsl"
#include "CommonTexture.hlsl"
#include "CommonShadow.hlsl"

struct VSIn 
{ 
    float3 pos : POSITION; 
    float3 n : NORMAL; 
    float2 uv : TEXCOORD; 
};

struct VSOut 
{ 
    float4 pos      : SV_POSITION; 
    float3 posWS    : TEXCOORD0;
    float3 normalWS : TEXCOORD1;
    float2 uv       : TEXCOORD2;
};

VSOut VSMain(VSIn vin)
{
    VSOut o;
    o.pos = mul(float4(vin.pos, 1.f), gWorldViewProj);
    float4 posWS4 = mul(float4(vin.pos, 1.f), gWorld);
    o.posWS = posWS4.xyz;

    o.normalWS = mul(float4(vin.n, 1.f), gWorld).xyz;
    o.uv = vin.uv;
    return o;
}

float4 PSMain(VSOut pin) : SV_TARGET
{
    float3 normal = normalize(pin.normalWS);
    float3 view = normalize(gCamPos - pin.posWS);



    float4 baseColor = gBaseColor;
    baseColor *= ParamTexture[0].Sample(CommonSampler, pin.uv); //텍스쳐 설정해줄 것
#if DEBUG_VIEW == 1
    return baseColor;
#elif DEBUG_VIEW == 2
    return float4(N * 0.5f + 0.5f, 1.0f);
#elif DEBUG_VIEW == 3
    return float4(gRoughness.xxx, 1.0f);
#elif DEBUG_VIEW == 4
    return float4(gMetallic.xxx, 1.0f);
#else
    float3 lit = 0;

    // Directional Lights
    [unroll]
    for (int i = 0; i < gDirCount; i++)
    {

        float3 dir = normalize(-gDirLights[i].direction);
        float shadow = SampleDirShadowPCF(i, pin.posWS, normal, dir, gDirLights[i].lightViewProj);
		float3 phong = EvaluateBlinnPhong(baseColor.rgb, normal, dir, view, gDirLights[i].color, gDirLights[i].intensity, gKs, gShininess);
		lit += phong * min(shadow + 0.1, 1);
    }

    // 아주 기본적인 환경/앰비언트(임시)
    lit += baseColor.rgb * 0.02f;

    return float4(lit, baseColor.a);
#endif
}