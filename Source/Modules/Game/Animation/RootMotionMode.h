#pragma once
#include <cstdint>

enum class ERootMotionMode : uint8_t
{
    NoRootMotionExtraction = 0,
    IgnoreRootMotion,
    RootMotionFromMontagesOnly,
    RootMotionFromEverything
};

enum class ERootLockMode : uint8_t
{
    RefPose = 0,
    AnimFirstFrame,
    Zero
};