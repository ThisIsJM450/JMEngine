#pragma once
#include <memory>
#include <string>

struct AnimSequenceAsset;
struct Skeleton;

class FBXImporter_Animation
{
public:
    static std::shared_ptr<AnimSequenceAsset> ImportAnimSequence_SectionKeyTransform(const std::string& fbxPath, const Skeleton& targetSkeleton, float sampleRate = 30.f);
};
