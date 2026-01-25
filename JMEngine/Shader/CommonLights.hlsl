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

    float  ShadowBias;
    float  NormalBias;
    float2 _pad0;       // float _pad0[2]
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

#endif //LIGHTING_COMMON_HLSLI