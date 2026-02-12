#include "CommonCBuffers.hlsl"
#include "CommonTexture.hlsl"
#include "CommonShadow.hlsl"
#include "CommonIBL.hlsl"

struct VSIn 
{ 
    float3 pos : POSITION; 
    float3 n : NORMAL; 
    float2 uv : TEXCOORD; 
    float4 tangent : TANGENT;
};

struct VSOut 
{ 
    float4 pos      : SV_POSITION; 
    float3 posWS    : TEXCOORD0;
    float3 normalWS : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    
    float4 tangentWS : TEXCOORD3;
    float3 bitanWS   : TEXCOORD4;
};

VSOut VSMain(VSIn vin)
{
    VSOut o;

    o.pos = mul(float4(vin.pos, 1.f), gWorldViewProj);

    float4 posWS4 = mul(float4(vin.pos, 1.f), gWorld);
    o.posWS = posWS4.xyz;

    // normal: w=0 (방향벡터)
    float3 N = normalize(mul(float4(vin.n, 0.f), gWorld).xyz);

    // tangent: xyz만 변환 + w(sign)은 그대로 유지
    float3 T = normalize(mul(float4(vin.tangent.xyz, 0.f), gWorld).xyz);

    // 직교화(중요)
    T = normalize(T - N * dot(N, T));

    float sign = vin.tangent.w;

    o.normalWS  = N;
    o.tangentWS = float4(T, sign);

    // bitangent는 cross로 재구성(안정)
    o.bitanWS = normalize(cross(N, T)) * sign;

    o.uv = vin.uv;

    return o;
}

void SampleMetalRough(float2 uv, out float metallic, out float roughness)
{
    float3 mr = ParamTexture[2].Sample(CommonSampler, uv).rgb;
    
    // Gloss는 1-mr값 사용
    if (gUseGlossMap != 0)
    {
        mr = 1 - mr;
    }

    //하나의 텍스쳐에 들거가있는지 확인
    if (gPackedMR_GB != 0)
    {
        // glTF MR 패킹: G=Roughness, B=Metallic
        roughness = mr.g * gRoughnessFactor;
        metallic  = mr.b * gMetallicFactor;
    }
    else
    {
        // 분리 metallic로 가정: R=Metallic, roughness는 factor만(또는 별도 rough 텍스처가 있으면 그쪽으로)
        metallic  = mr.r * gMetallicFactor;
        
        roughness = ParamTexture[5].Sample(CommonSampler, uv).r * gRoughnessFactor;
    }
}


float4 PSMain(VSOut pin) : SV_TARGET
{
    float3 V = normalize(gCamPos - pin.posWS);

    // BaseColor
    float4 baseTex = ParamTexture[0].Sample(CommonSampler, pin.uv);
    float4 baseColor = gBaseColorFactor * baseTex;

    //return float4(pin.uv, 0, 1);
#if DEBUG_VIEW == 1
    return baseColor;
#endif

    // Normalmap Texture를 통해 NormalVector 세팅
    float3 N = normalize(pin.normalWS);
    if (gUseNormalMap != 0)
    {
        N = ApplyNormalMap(ParamTexture[1].Sample(CommonSampler, pin.uv).xyz, gNormalScale, N, pin.tangentWS, pin.bitanWS);
    }

#if DEBUG_VIEW == 2
    return float4(N * 0.5f + 0.5f, 1.0f);
#endif

    // Metallic / Roughness
    float metallic, roughness;
    SampleMetalRough(pin.uv, metallic, roughness);

#if DEBUG_VIEW == 3
    return float4(roughness.xxx, 1.0f);
#elif DEBUG_VIEW == 4
    return float4(metallic.xxx, 1.0f);
#endif

    // AO / Emissive
    float ao = ParamTexture[3].Sample(CommonSampler, pin.uv).r * gAOFactor;
    float3 emissive = ParamTexture[4].Sample(CommonSampler, pin.uv).rgb * gEmissiveFactor;

    float3 lit = 0;

    // Directional Lights
    [unroll]
    for (int i = 0; i < gDirCount; i++)
    {
        float3 L = normalize(-gDirLights[i].direction);

        float shadow = SampleDirShadowPCF(i, pin.posWS, N, L, gDirLights[i].lightViewProj);

        float3 pbr = EvaluatePBR_Direct(baseColor.rgb, metallic, roughness,
            N, V, L,gDirLights[i].color, gDirLights[i].intensity);

        lit += pbr * min(shadow + 0.1, 1.0f);
    }
    
    // SpotLight
    [unroll]
    for (int i = 0; i < gSpotCount; i++)
    {
        lit += EvaluateSpotOrPointPBR(
        gSpotLights[i],
        pin.posWS, N, V,
        baseColor.rgb, metallic, roughness);
    }

    // emissive 추가
    lit += emissive;
    
    // IBL
    lit += EvaluateIBL(N, V, baseColor, metallic, roughness, ao);

    return float4(lit, baseColor.a);
}