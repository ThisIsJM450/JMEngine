#pragma once

#include "SceneComponent.h"
#include "../../Renderer/RenderTypes.h"

class PointLightComponent : public SceneComponent
{
public:
    PointLightComponent();

    void OnRegister() override;
    void OnUnregister() override;

    SpotLight GetLightData() const;

    float range = 10.0f;
    DirectX::XMFLOAT3 color {1, 1, 1};
    float intensity = 1.0f;

    bool castShadow = true;
    float shadowBias = 0.001f;
    float normalBias = 0.0f;
};

