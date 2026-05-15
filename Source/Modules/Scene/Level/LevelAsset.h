#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

struct LevelActorDesc
{
    std::string type;
    std::string className;
    std::string name;
    std::unordered_map<std::string, std::string> properties;

    DirectX::XMFLOAT3 position{0.f, 0.f, 0.f};
    DirectX::XMFLOAT3 rotationEuler{0.f, 0.f, 0.f};
    DirectX::XMFLOAT3 scale{1.f, 1.f, 1.f};

    std::string meshReference;
    float meshParam = 0.f;
    uint32_t meshStacks = 0;
    uint32_t meshSlices = 0;
    std::string materialReference;

    float cameraFovDegree = 60.f;
    float cameraNearZ = 0.1f;
    float cameraFarZ = 1000.f;
    bool bSetAsActiveCamera = true;

    DirectX::XMFLOAT3 lightColor{1.f, 1.f, 1.f};
    float lightIntensity = 1.f;
    float lightRange = 10.f;
    float lightSpotAngleDegrees = 30.f;
};

struct LevelAsset
{
    std::string name;
    std::string requestedActiveCameraName;
    std::vector<LevelActorDesc> actors;

    static bool LoadFromFile(const std::string& path, LevelAsset& outAsset);
    static bool SaveToFile(const std::string& path, const LevelAsset& asset);
};
