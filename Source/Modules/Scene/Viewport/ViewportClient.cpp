#include "ViewportClient.h"

/*
void ViewportClient::Tick(float dt, const Input& input)
{
    Camera* Camera = GetCamera();
    if (Camera == nullptr)
    {
        return;
    }
    
    if (input.RMBDown())
    {
        Camera->
        rotation.yaw   += in.MouseDeltaX() * lookSpeed;
        rotation.pitch += in.MouseDeltaY() * lookSpeed;
        rotation.pitch = Clamp(rotation.pitch, -89.f, 89.f);
    }

    Vec3 fwd = ForwardFromYawPitch(rotation);
    Vec3 right = Normalize(Cross(fwd, Vec3(0,1,0)));

    float spd = moveSpeed * (in.ShiftDown() ? 3.0f : 1.0f);
    if (in.KeyDown('W')) position += fwd * spd * dt;
    if (in.KeyDown('S')) position -= fwd * spd * dt;
    if (in.KeyDown('A')) position -= right * spd * dt;
    if (in.KeyDown('D')) position += right * spd * dt;
    if (in.KeyDown('Q')) position -= Vec3(0,1,0) * spd * dt;
    if (in.KeyDown('E')) position += Vec3(0,1,0) * spd * dt;
}

Camera* ViewportClient::GetCamera() const
{
    if (m_scene.expired() == false)
    {
        return &m_scene.lock()->GetCamera();
    }

    return nullptr;
}
*/
