#pragma once
#include <string>
#include <vector>

#include "RootMotionMode.h"
#include "../../Core/Transform.h"

struct AnimSection
{
    std::string Name;

    float StartSec = 0.f;
    float LengthSec = 0.f;

    float SampleRate = 30.f;
    uint32_t NumFrames = 0;

    std::vector<Transform> Keys;

    // Root motion 
    bool bEnableRootMotion = false;
    bool bForceRootLock = true;
    bool bFromMontage = false;
    ERootLockMode RootLockMode = ERootLockMode::RefPose;

    Transform GetKey(uint32_t frame, uint32_t bone, uint32_t boneCount) const
    {
        return Keys[frame * boneCount + bone];
    }
};

struct AnimSequenceAsset
{
    std::string Name;
    uint64_t SkeletonHash = 0;

    uint32_t BoneCount = 0;
    float TotalLengthSec = 0.f;

    std::vector<AnimSection> Sections;
    
    int FindSectionIndex(const std::string& sectionName) const
    {
        for (int i = 0; i < (int)Sections.size(); ++i)
            if (Sections[i].Name == sectionName) return i;
        return -1;
    }
};
