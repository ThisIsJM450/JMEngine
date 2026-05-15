#pragma once

#include "ActorComponent.h"
#include "../../Core/Transform.h"
#include "../Animation/RootMotionMode.h"

class Character;
class SkeletalMeshComponent;
class AnimInstance;

class CharacterMovementComponent : public ActorComponent
{
public:
    CharacterMovementComponent();

    void Tick(float deltaTime) override;

    void SetRootMotionMode(ERootMotionMode InMode) { m_RootMotionMode = InMode; }
    ERootMotionMode GetRootMotionMode() const { return m_RootMotionMode; }
    void ForceResetRootMotionState();

private:
    bool ShouldApplyRootMotion(const AnimInstance* Anim) const;
    void ConsumeAndApplyRootMotion(float dt);
    void ResetRootMotionAccumulator();

    // 추후 sweep/slide 충돌 경로와 연결할 함수 경계
    bool MoveWithCollision(const Transform& Delta);
    void ApplyRootMotionDeltaRaw(const Transform& Delta);

private:
    ERootMotionMode m_RootMotionMode = ERootMotionMode::RootMotionFromEverything;
    bool m_bRootMotionAccumInitialized = false;
    Transform m_RootMotionBase;
    Transform m_RootMotionAccum;
};

