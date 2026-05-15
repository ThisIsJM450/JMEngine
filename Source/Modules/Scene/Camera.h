#pragma once
#include <DirectXMath.h>
#include <__msvc_string_view.hpp>

class Camera
{
public:
    void SetPerspective(float fovDegree, float aspect, float nearZ, float fearZ);
    void LookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& up);

    DirectX::XMMATRIX GetView() const { return m_View; }
    DirectX::XMMATRIX GetProj() const { return m_Proj; }
    DirectX::XMFLOAT3 GetPosition() const;

    void AddYawPitch(float deltaX, float deltaY, float degPerPixel);
    void MoveLocal(float forward, float strafe, float up, float speed, float dt);

public:
    DirectX::XMVECTOR GetForward() const;
    DirectX::XMVECTOR GetRight() const;
    void UpdateView();
    
private:
    DirectX::XMMATRIX m_View = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX m_Proj = DirectX::XMMatrixIdentity();

    DirectX::XMFLOAT3 Position { 0.0f, 1.3f, -3.2f };
    float YawDeg   = 0.0f;   // 좌우
    float PitchDeg = 0.0f;   // 상하 (-89~89 권장)
};


