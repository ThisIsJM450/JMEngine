#pragma once

#include <DirectXMath.h>

/**
 * Material 파라미터(상수버퍼) 정의
 */

// 16바이트 정렬 유지(상수버퍼 규칙)
struct alignas(16) CBMaterial
{
    DirectX::XMFLOAT4 BaseColor = {1,1,1,1};
    float Roughness = 0.5f;
    float Metallic  = 0.3f;
    float Pad0 = 0.0f;
    float Pad1 = 0.0f;
};
static_assert((sizeof(CBMaterial) % 16) == 0, "CBMaterial must be 16-byte aligned");