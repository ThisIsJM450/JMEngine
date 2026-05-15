#include "CommonConst.hlsl"

TextureCube<float4> gCubeMap : register(t10);
TextureCube<float4> gIrradianceCube : register(t11);
TextureCube<float4> gPrefilterEnv : register(t12);
Texture2D<float2>   gBRDFLUT      : register(t13);
SamplerState CubeMapSampler : register(s2); // linear clamp

float3 ApplyDiffuseIBL(
    float3 N,          // world normal (normalized)
    float3 albedo,     // linear albedo
    float3 kD,
    float ao           // optional (1이면 무시)
)
{
    // Irradiance cubemap은 N 방향으로 샘플링
    float3 irradiance = gIrradianceCube.SampleLevel(CubeMapSampler, N, 0).rgb;

    // Lambert diffuse = albedo / PI
    float3 diffuseIBL = irradiance * (albedo / PI) * kD;

    // AO 적용
    diffuseIBL *= ao;

    return diffuseIBL;
}

float3 ApplySpecularIBL(
    float3 N,
    float3 V,
    float3 F,
    float  roughness
)
{
    float prefilterMipCount = 9;
    N = normalize(N);
    V = normalize(V);

    float NdotV = saturate(dot(N, V));

    // 반사 방향
    float3 R = reflect(-V, N);

    // roughness -> mip
    // (보통 mipCount-1 범위로 매핑)
    float mip = roughness * (prefilterMipCount - 1);

    // prefiltered env 샘플 (LOD 고정)
    float3 prefilteredColor = gPrefilterEnv.SampleLevel(CubeMapSampler, R, mip).rgb;

    // BRDF LUT 샘플
    float2 brdf = gBRDFLUT.Sample(CubeMapSampler, float2(NdotV, roughness)).rg;

    // split-sum
    float3 specIBL = prefilteredColor * (F * brdf.x + brdf.y);

    return specIBL;
}

float3 EvaluateIBL(float3 N_ws,
    float3 V_ws,
    float3 albedo,     // linear
    float  metallic,
    float  roughness,
    float  ao)
{
    N_ws = normalize(N_ws);
    V_ws = normalize(V_ws);

    float NdotV = saturate(dot(N_ws, V_ws));

    // F0 (metalness workflow)
    float3 F0 = ComputeF0(albedo, metallic);

    // Fresnel (간접광용)
    float3 F = FresnelSchlickRoughness(F0, NdotV, roughness);

    // Energy conservation
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float3 diffuseIBL = ApplyDiffuseIBL(N_ws, albedo, kD, ao);
    float3 specIBL    = ApplySpecularIBL(N_ws, V_ws, F, roughness);

    return diffuseIBL + specIBL;
}
