#include "CommonCBuffers.hlsl"
#include "CommonIBL.hlsl"


struct VSIn 
{ 
    float3 pos : POSITION; 
    float3 n : NORMAL; 
    float2 uv : TEXCOORD; 
};

struct VSOut
{
    float4 posH    : SV_Position;
    float3 dirW : TEXCOORD0;  // 추가
};

VSOut VSMain(VSIn vin)
{
    VSOut vout;

    // 카메라 회전만 적용 (translation 제거)
    float4x4 viewNoTrans = gView;
    viewNoTrans._41 = 0.0;
    viewNoTrans._42 = 0.0;
    viewNoTrans._43 = 0.0;

    // 큐브 정점 방향을 월드/뷰 회전에 맞춰 샘플링 방향으로 사용
    float3 dir = vin.pos;

    // 스카이 박스는 멀리 있다고 가정하고 위치는 시야에만 맞춰줌
    float4 posV = mul(float4(vin.pos, 1.0), viewNoTrans);
    float4 posH = mul(posV, gProj);

    // 깊이 문제 방지: 항상 가장 뒤에 그려지도록 (z = w)
    posH.z = posH.w;

    vout.posH = posH;
    vout.dirW = dir; // 이 dir은 큐브 메쉬가 카메라 중심에 있다고 가정한 방향

    return vout;
}

float4 PSMain(VSOut pin) : SV_Target
{
    float3 dir = normalize(pin.dirW);
    return gCubeMap.SampleLevel(CubeMapSampler, dir, 0);
}