#pragma once
#include <string>
#include <vector>

#include "../../Core/Transform.h"

struct AnimSection
{
    std::string Name;

    float StartSec = 0.f;      // sequence time 기준
    float LengthSec = 0.f;

    float SampleRate = 30.f;   // 고정 프레임
    uint32_t NumFrames = 0;    // = floor(LengthSec*SampleRate)+1

    // frame-major: [frame][bone]
    // index = frame * BoneCount + bone
    std::vector<Transform> Keys;
    
    
    //std::vector<Transform> RefPose;

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
