#pragma once

#include "SceneComponent.h"
#include "../../Renderer/RenderTypes.h"

class DirectionalLightComponent : public SceneComponent
{
public:

    void OnRegister() override;
    void OnUnregister() override;
    
    DirectionalLight GetLightData() const
    {
        DirectionalLight l{};
        // Directional은 “월드 forward”를 라이트 방향으로 사용
        DirectX::XMVECTOR f = GetOwner()->GetRootComponent()->GetWorldForward();
        DirectX::XMFLOAT3 dir;
        DirectX::XMStoreFloat3(&dir, DirectX::XMVector3Normalize(f));
        DirectX::XMFLOAT3 up;
        DirectX::XMStoreFloat3(&up, GetOwner()->GetRootComponent()->GetWorldUp());
        
        l.direction = dir;
        l.up = up;
        l.color = color;
        l.intensity = intensity;
        l.castShadow = castShadow ? 1 : 0;
        l.ShadowBias = shadowBias;
        l.NormalBias = normalBias;
        return l;
    }

    DirectX::XMFLOAT3 color {1,1,1};
    float intensity = 1.0f;
    bool castShadow = true;
    float shadowBias = 0.0005f;
    float normalBias = 0.0f;
};
