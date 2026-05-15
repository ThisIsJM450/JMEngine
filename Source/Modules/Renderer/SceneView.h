#pragma once
#include <DirectXMath.h>

struct ViewInfo
{
    DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX viewProj = DirectX::XMMatrixIdentity();

    DirectX::XMFLOAT3 cameraPosition = DirectX::XMFLOAT3(0, 0, 0);
    float _pad0 = 0.f;
};

struct SceneView : public ViewInfo
{
    int width = 0;
    int height = 0;
};
