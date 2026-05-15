#include "LevelAsset.h"

#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
    DirectX::XMFLOAT3 ReadFloat3(const json& j, const char* key, const DirectX::XMFLOAT3& defaultValue)
    {
        if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3)
        {
            return defaultValue;
        }

        return DirectX::XMFLOAT3(
            j[key][0].get<float>(),
            j[key][1].get<float>(),
            j[key][2].get<float>());
    }
}

bool LevelAsset::LoadFromFile(const std::string& path, LevelAsset& outAsset)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        return false;
    }

    json root;
    ifs >> root;

    outAsset = LevelAsset{};
    outAsset.name = root.value("name", "UnnamedLevel");
    outAsset.requestedActiveCameraName = root.value("activeCamera", "");

    if (!root.contains("actors") || !root["actors"].is_array())
    {
        return true;
    }

    for (const json& a : root["actors"])
    {
        LevelActorDesc desc{};
        desc.type = a.value("type", "");
        desc.className = a.value("className", desc.type);
        desc.name = a.value("name", desc.type);
        if (a.contains("properties") && a["properties"].is_object())
        {
            for (auto it = a["properties"].begin(); it != a["properties"].end(); ++it)
            {
                if (it.value().is_string())
                {
                    desc.properties[it.key()] = it.value().get<std::string>();
                }
            }
        }
        desc.position = ReadFloat3(a, "position", {0.f, 0.f, 0.f});
        desc.rotationEuler = ReadFloat3(a, "rotationEuler", {0.f, 0.f, 0.f});
        desc.scale = ReadFloat3(a, "scale", {1.f, 1.f, 1.f});

        desc.meshReference = a.value("meshReference", a.value("meshShape", ""));
        desc.meshParam = a.value("meshParam", 0.0f);
        desc.meshStacks = a.value("meshStacks", 0u);
        desc.meshSlices = a.value("meshSlices", 0u);
        desc.materialReference = a.value("materialReference", a.value("material", ""));

        desc.cameraFovDegree = a.value("cameraFovDegree", 60.0f);
        desc.cameraNearZ = a.value("cameraNearZ", 0.1f);
        desc.cameraFarZ = a.value("cameraFarZ", 1000.0f);
        desc.bSetAsActiveCamera = a.value("setAsActiveCamera", true);

        desc.lightColor = ReadFloat3(a, "lightColor", {1.f, 1.f, 1.f});
        desc.lightIntensity = a.value("lightIntensity", 1.0f);
        desc.lightRange = a.value("lightRange", 10.0f);
        desc.lightSpotAngleDegrees = a.value("lightSpotAngleDegrees", 30.0f);

        outAsset.actors.push_back(desc);
    }

    return true;
}

bool LevelAsset::SaveToFile(const std::string& path, const LevelAsset& asset)
{
    json root;
    root["name"] = asset.name;
    if (!asset.requestedActiveCameraName.empty())
    {
        root["activeCamera"] = asset.requestedActiveCameraName;
    }

    root["actors"] = json::array();
    for (const LevelActorDesc& actor : asset.actors)
    {
        json a;
        a["type"] = actor.type;
        if (!actor.className.empty()) a["className"] = actor.className;
        a["name"] = actor.name;
        if (!actor.properties.empty()) a["properties"] = actor.properties;
        a["position"] = { actor.position.x, actor.position.y, actor.position.z };
        a["rotationEuler"] = { actor.rotationEuler.x, actor.rotationEuler.y, actor.rotationEuler.z };
        a["scale"] = { actor.scale.x, actor.scale.y, actor.scale.z };

        if (!actor.meshReference.empty()) a["meshReference"] = actor.meshReference;
        if (actor.meshParam != 0.0f) a["meshParam"] = actor.meshParam;
        if (actor.meshStacks != 0u) a["meshStacks"] = actor.meshStacks;
        if (actor.meshSlices != 0u) a["meshSlices"] = actor.meshSlices;
        if (!actor.materialReference.empty()) a["materialReference"] = actor.materialReference;

        a["cameraFovDegree"] = actor.cameraFovDegree;
        a["cameraNearZ"] = actor.cameraNearZ;
        a["cameraFarZ"] = actor.cameraFarZ;
        a["setAsActiveCamera"] = actor.bSetAsActiveCamera;

        a["lightColor"] = { actor.lightColor.x, actor.lightColor.y, actor.lightColor.z };
        a["lightIntensity"] = actor.lightIntensity;
        a["lightRange"] = actor.lightRange;
        a["lightSpotAngleDegrees"] = actor.lightSpotAngleDegrees;

        root["actors"].push_back(std::move(a));
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        return false;
    }

    ofs << root.dump(2);
    return ofs.good();
}
