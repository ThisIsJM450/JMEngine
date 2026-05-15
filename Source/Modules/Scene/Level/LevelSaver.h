#pragma once

#include <string>

struct LevelAsset;
class World;

class LevelSaver
{
public:
    static LevelAsset BuildLevelAssetFromWorld(const World& world, const std::string& levelName = "SavedLevel");
    static bool SaveWorldToFile(const World& world, const std::string& path, const std::string& levelName = "SavedLevel");
};
