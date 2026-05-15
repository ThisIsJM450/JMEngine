#include "CommonConst.hlsl"
#ifndef kMaxDirLights
#define kMaxDirLights 2
#endif //kMaxDirLights

#ifndef kMaxSpotLights
#define kMaxSpotLights 8
#endif //kMaxSpotLights

#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI
struct DirectionalLight
{
    float4x4 lightViewProj; 
    float3 direction;   // XMFLOAT3
    float  intensity;

    float3 color;       // XMFLOAT3
    int    castShadow;  // int (32-bit)

    float3 up;
    float  ShadowBias;
    float  NormalBias;
    float3 _pad0;       // float _pad0[2]
};

struct SpotLight
{
    float3 position;
    float  range;

    float3 direction;
    float  spotAngleRadians;

    float3 color;
    float  intensity;

    int    castShadow;
    float  ShadowBias;
    float  normalBias;
    float  _pad1;
};

// Lambert(확산) 반사 계산.
// - albedo: 표면의 기본 색(확산 반사 색)
// - N: 표면 법선(정규화된 방향 벡터라고 가정)
// - L: 광원 방향(정규화된 방향 벡터라고 가정, 보통 "표면->광원" 방향)
// - lightColor: 광원의 색(RGB)
// - intensity: 광원의 세기(밝기 스케일)
float3 EvaluateLambert(float3 albedo, float3 N, float3 L, float3 lightColor, float intensity)
{
    // N·L = cos(theta)
    // - 표면이 빛을 얼마나 정면으로 받는지(각도에 따른 밝기) 계산
    // - 음수면 빛이 표면 뒤쪽에서 오는 것이므로 기여 0
    // saturate(x) = clamp(x, 0, 1)
    float ndotl = saturate(dot(N, L));

    // Lambert diffuse = albedo * (lightColor * intensity) * max(0, N·L)
    // - albedo: 재질이 확산으로 반사하는 색
    // - lightColor*intensity: 광원이 주는 RGB 에너지
    // - ndotl: 각도에 따른 감쇠(정면=1, 비스듬=0~1)
    return albedo * lightColor * intensity * ndotl;
}

// Blinn-Phong 반사 계산
// Lambert(확산)에 Specular(정반사)를 더한 모델
// Half Vector를 통해 계산 최적화 (Blinn)
float3 EvaluateBlinnPhong(float3 albedo, float3 N, float3 L, float3 V, float3 lightColor, float intensity, float ks, float shininess)
{
    // Diffuse term (Lambert)
    float3 diffuse = albedo * saturate(dot(N, L)) * lightColor * intensity;
    
    // Specular term (Blinn-Phong)
    float3 H = normalize(V + L); // Normal과 Light의 HalfVector
    float ndoth = saturate(dot(N, H));
    float spec  = ks * pow(ndoth, shininess);
    
    // Specular is usually tinted by light color (and intensity)
    float3 specular = lightColor * intensity * spec;
    
    return diffuse + specular;
}


// ~ Start Normal map 적용

float3 DecodeNormalTS(float3 n)
{
    // BC5/일반 노멀맵 모두 대응: [0,1] -> [-1,1]
    float3 nn = n * 2.0f - 1.0f;
    return nn;
}

float3 ApplyNormalMap(float3 NormalMapValue, float NormalScale, float3 N, float4 T, float3 B)
{
    float3 nTS = DecodeNormalTS(NormalMapValue);
    nTS.xy *= NormalScale;
    nTS = normalize(nTS);


    float3 Nw = normalize(N);
    float3 Tw = normalize(T);

    // T를 N에 직교화(보간/오차로 N과 기울어지는 문제 방지)
    Tw = normalize(Tw - Nw * dot(Nw, Tw));

    // B는 cross로 재구성 + sign 적용(미러 UV/좌표계 보정)
    float3 Bw = normalize(cross(Nw, Tw));

    float3x3 TBN = float3x3(Tw, Bw, Nw);
    float3 nW = normalize(mul(nTS, TBN));
    if (dot(nW, Nw) < 0) nW = -nW;   // 임시 안전핀
    return nW;
}
// ~ End Normal map 적용

// ~ Start PBR BDRF
// ------------------------------------------------------------
// Metalness workflow에서 F0(기본 방사율)를 계산 
// ------------------------------------------------------------
float3 ComputeF0(float3 albedo, float metallic)
{
    return lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
}

// ------------------------------------------------------------
// Fresnel (F) : Schlick 근사
// - 시선 방향(V)과 하프 벡터(H)의 각도에 따라 반사율이 증가하는 현상(가장자리에서 더 반짝임)
// - F0는 "정면(수직)에서의 반사율" (dielectric은 보통 약 0.04)
// ------------------------------------------------------------
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // cosTheta = dot(V, H) (또는 dot(N, V) 기반의 변형도 있음)
    // (1 - cosTheta)^5 형태로 grazing angle에서 급격히 1에 가까워짐
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

// roughness 고려한 fresnel(간접광에서 흔히 씀)
float3 FresnelSchlickRoughness(float3 F0, float cosTheta, float roughness)
{
    float3 Fr = max(1.0 - roughness, F0);
    return F0 + (Fr - F0) * pow(1.0 - cosTheta, 5.0);
}

// ------------------------------------------------------------
// Normal Distribution Function (D) : Trowbridge-Reitz GGX
// - "하프 벡터(H) 방향으로 정렬된 마이크로패싯이 얼마나 있는가"를 통계적으로 근사
// - roughness가 작을수록(매끈) 분포가 날카로워져 specular highlight가 작고 강해짐
// - roughness가 클수록(거침) 분포가 퍼져 highlight가 넓고 흐려짐
// ------------------------------------------------------------
float DistributionGGX(float NdotH, float roughness)
{
    // UE4 관례: a = roughness^2 로 remap해서 거칠기 반응을 더 안정적으로
    float a  = roughness * roughness;
    float a2 = a * a;

    // GGX TR: a^2 / ( PI * ((N·H)^2 (a^2-1) + 1)^2 )
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denom * denom, 1e-6);
}

// ------------------------------------------------------------
// Geometry term (G의 부분) : Schlick-GGX (direct lighting용 k)
// - 마이크로패싯들이 서로 가려서(Shadowing/Masking) 실제로 보이는/빛을 받는 비율을 근사
// - NdotV, NdotL 각각에 대해 계산한 뒤 Smith 방식으로 곱해서 사용
// - k 값은 direct lighting과 IBL에서 다른 remap을 쓰는 경우가 많음
//   여기서는 direct lighting용 k = ( (roughness+1)^2 ) / 8 사용
// ------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;

    // direct lighting용 k (UE4 방식)
    // roughness가 커질수록 기하 감쇠가 강해져 specular 에너지가 과하게 나오지 않게 도움
    float k = (r * r) / 8.0f;

    // G1(N,V) = NdotV / (NdotV*(1-k) + k)
    return NdotV / max(NdotV * (1.0f - k) + k, 1e-6);
}

// ------------------------------------------------------------
// Geometry term (G) : Smith method
// - View쪽 가림(Obstruction/Masking)과 Light쪽 가림(Shadowing)을 각각 구해 곱함
// - 결과는 0~1 범위, 1이면 가림 없음
// ------------------------------------------------------------
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggxV = GeometrySchlickGGX(NdotV, roughness); // G1(N,V)
    float ggxL = GeometrySchlickGGX(NdotL, roughness); // G1(N,L)
    return ggxV * ggxL;                                // G = G1V * G1L
}

// ------------------------------------------------------------
// Cook-Torrance BRDF (Direct Lighting)
// fr = kD * Lambert + SpecularCookTorrance
//
// SpecularCookTorrance = (D * G * F) / (4 (N·V)(N·L))
// D: GGX NDF
// G: Smith (Schlick-GGX)
// F: Fresnel-Schlick
//
// Energy conservation:
// kS = F (반사 비율)
// kD = (1 - kS) * (1 - metallic)  (비금속만 diffuse 유지)
// ------------------------------------------------------------
float3 EvaluatePBR( float3 baseColor, float metallic, float roughness,
                            float3 N, float3 V, float3 L, float3 radiance)
{
    // Half vector: Blinn-Phong에서 쓰던 H와 동일한 개념이지만
    // PBR에서는 D/F 계산에 핵심적으로 사용됨
    float3 H = normalize(V + L);

    // 기본 도트값들 (clamp to [0,1])
    float NdotL = saturate(dot(N, L)); // 입사각 (빛 방향)
    float NdotV = saturate(dot(N, V)); // 시선각 (뷰 방향)
    float NdotH = saturate(dot(N, H)); // NDF에 사용
    float VdotH = saturate(dot(V, H)); // Fresnel에 사용(= cosTheta)

    // roughness가 0에 가까우면 D가 폭발하거나(엄청 날카로움),
    // 실시간에서는 aliasing/노이즈가 심해지기 쉬워 하한을 둠
    roughness = max(roughness, 0.04f);

    // F0(정면 반사율) 결정:
    // - 비금속(dielectric): 보통 0.04 근처(플라스틱/나무/피부 등)
    // - 금속(metal): baseColor가 곧 specular 색(F0) 역할(금속은 colored specular)
    float3 F0 = ComputeF0(baseColor, metallic);

    // Cook-Torrance 3요소
    float  D = DistributionGGX(NdotH, roughness);          // microfacet 분포
    float  G = GeometrySmith(NdotV, NdotL, roughness);     // microfacet 가림(Shadowing/Masking)
    float3 F = FresnelSchlick(VdotH, F0);                  // 각도별 반사율

    // Specular BRDF:
    // 분모의 4(N·V)(N·L)은 마이크로패싯 BRDF의 정규화 항
    float3 spec = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-6);

    // Energy conservation 분해:
    // kS : 반사(= specular) 비율
    // kD : 굴절/산란(= diffuse) 비율 (금속은 diffuse가 거의 없음)
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);

    // Lambert diffuse (정규화 /PI)
    float3 diffuse = kD * baseColor / PI;

    // Direct lighting 최종:
    // BRDF(fr) * Radiance * (N·L)
    return (diffuse + spec) * radiance * NdotL;
}

float3 EvaluatePBR_Direct( float3 baseColor, float metallic, float roughness,
                            float3 N, float3 V, float3 L, float3 lightColor, float lightIntensity)
{
    // 광원 복사/강도(라디언스). (색 * intensity)
    float3 radiance = lightColor * lightIntensity;
    return EvaluatePBR(baseColor, metallic, roughness, N, V, L, radiance);
}

// ~ End PBR BRDF (GGX/Schlick/Smith)

//~ Start SpotLight
bool IsPointLikeSpot(float3 dir)
{
    return dot(dir, dir) < 1e-8f; // 거의 0이면 Point로 취급
}

float SpotConeFactor(float3 L, float3 spotDir, float spotAngleRadians)
{
    // dir이 0이면 cone=1 (Point처럼)
    if (IsPointLikeSpot(spotDir)) return 1.0f;

    float3 D = normalize(spotDir);
    float cosTheta = dot(normalize(-L), D); // -L = Light->P

    float outerCos = cos(spotAngleRadians * 0.5f);
    float innerCos = cos(spotAngleRadians * 0.5f * 0.85f); // 부드러운 경계용(임의)

    return saturate((cosTheta - outerCos) / max(innerCos - outerCos, 1e-4));
}

float Attenuation_Range(float dist, float range)
{
    float x = saturate(1.0 - dist / range);
    return x * x;
}

float Attenuation_Point(float dist, float range)
{
    float invSq = 1.0 / max(dist * dist, 1e-4);
    return invSq * Attenuation_Range(dist, range);
}

float3 EvaluateSpotOrPointPBR(SpotLight light,
    float3 P, float3 N, float3 V,
    float3 baseColor, float metallic, float roughness)
{
    float3 toL = light.position - P;
    float dist = length(toL);
    if (dist >= light.range)
    {
        return float3(0,0,0);
    }

    float3 L = toL / max(dist, 1e-4);

    float att = Attenuation_Point(dist, light.range);
    
    float cone = SpotConeFactor(L, light.direction, light.spotAngleRadians);
    if (cone <= 0.0f)
    {
        return float3(0,0,0);
    }

    float shadow = 1.0f;
    
    float3 radiance = light.color * (light.intensity * att);
    float3 pbr = EvaluatePBR(
        baseColor, metallic, roughness,
        N, V, L, radiance);

    return pbr * shadow;
}
//~ End SpotLight


#endif //LIGHTING_COMMON_HLSLI