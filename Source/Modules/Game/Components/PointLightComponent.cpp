#include "PointLightComponent.h"

#include "../World.h"

PointLightComponent::PointLightComponent() : SceneComponent()
{
    TypeName = std::string("PointLightComponent");
}

void PointLightComponent::OnRegister()
{
    World* world = GetWorld();
    if (!world) return;
    world->GetScene().AddPointLight(this);
}

void PointLightComponent::OnUnregister()
{
    World* world = GetWorld();
    if (!world) return;
    world->GetScene().RemovePointLight(this);
}

SpotLight PointLightComponent::GetLightData() const
{
    SpotLight l{};
    l.position = GetWorldLocation();
    l.direction = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
    l.range = range;
    l.spotAngleRadians = DirectX::XMConvertToRadians(179.0f);
    l.color = color;
    l.intensity = intensity;
    l.castShadow = castShadow ? 1 : 0;
    l.ShadowBias = shadowBias;
    l.normalBias = normalBias;
    l.IsPoint = 1.0f;
    return l;
}
