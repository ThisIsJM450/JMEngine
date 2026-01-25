Texture2D<float4> ShadowTexture[kMaxDirLights] : register(t8); 
SamplerComparisonState ShadowCmp : register(s1);

float SampleDirShadowPCF(int lightIndex, float3 posWS, float3 normalWS, float3 lightDirWS, float4x4 lightViewProj)
{

    float4 posLS = mul(float4(posWS, 1.0), lightViewProj);
    float3 ndc = posLS.xyz / posLS.w;

    // Light clip -> UV
    float2 uv = ndc.xy;
	uv.x = 0.5 * uv.x + 0.5f;
	uv.y = -0.5 * uv.y + 0.5f;
    float  depth = ndc.z;
    
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
	

    // Bias (slope 기반)
    float NoL = saturate(dot(normalWS, lightDirWS));
    float bias = max(0.0025, 0.01 * NoL);
	return ShadowTexture[lightIndex].SampleCmpLevelZero(ShadowCmp, uv , depth - bias);
}
