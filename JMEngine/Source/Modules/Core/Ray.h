#pragma once
#include <DirectXTex.h>

struct Ray
{
    DirectX::XMFLOAT3 origin;
    DirectX::XMFLOAT3 dir; // 반드시 normalize
};

struct AABB
{
    DirectX::XMFLOAT3 min;
    DirectX::XMFLOAT3 max;
};
