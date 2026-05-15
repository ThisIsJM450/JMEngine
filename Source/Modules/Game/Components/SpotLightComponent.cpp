#include "SpotLightComponent.h"
#include "../World.h"
#include "../Actor.h"

void SpotLightComponent::OnRegister()
{
    World* world = GetWorld();
    if (!world) return;
    world->GetScene().AddSpotLight(this);
}

void SpotLightComponent::OnUnregister()
{
    World* world = GetWorld();
    if (!world) return;
    world->GetScene().RemoveSpotLight(this);
}

SpotLight SpotLightComponent::GetLightData() const
{
    SpotLight l{};
    l.position = GetWorldLocation();

    DirectX::XMVECTOR f = GetWorldForward();
    DirectX::XMFLOAT3 dir;
    DirectX::XMStoreFloat3(&dir, DirectX::XMVector3Normalize(f));
    l.direction = dir;

    l.range = range;
    l.spotAngleRadians = spotAngleRadians;
    l.color = color;
    l.intensity = intensity;

    l.castShadow = castShadow;
    l.ShadowBias = shadowBias;
    l.normalBias = normalBias;
    return l;
}
