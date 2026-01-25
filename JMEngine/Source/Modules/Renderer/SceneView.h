#pragma once
#include <DirectXMath.h>

struct SceneView
{
    DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX viewProj = DirectX::XMMatrixIdentity();

    DirectX::XMFLOAT3 cameraPosition = DirectX::XMFLOAT3(0, 0, 0);
    float _pad0 = 0.f;

    int width = 0;
    int height = 0;
};
