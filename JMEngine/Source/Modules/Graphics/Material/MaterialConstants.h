#pragma once

#include <DirectXMath.h>

/**
 * Material 파라미터(상수버퍼) 정의
 */

// 16바이트 정렬 유지(상수버퍼 규칙)
struct alignas(16) CBMaterial
{
    DirectX::XMFLOAT4 BaseColor = {1,1,1,1};
    float Roughness = 0.9f;
    float Metallic  = 0.0f;
    float  NormalScale = 1.f;       // 기본 1 (원하면)
    float  AOFactor = 1.0f;          // 기본 1
    DirectX::XMFLOAT3 EmissiveFactor = {0, 0, 0};    // 기본 0
    
    float   PackedMR_GB = 0;       // 1이면 t2에서 G=Rough, B=Metal / 0이면 분리 metallic로 보고 R=Metal
    float   UseNormalMap = 0;      // 1이면 normal map 사용
    float   UseGlossMap = 0;
    float   gPad0 = 0;
};
static_assert((sizeof(CBMaterial) % 16) == 0, "CBMaterial must be 16-byte aligned");