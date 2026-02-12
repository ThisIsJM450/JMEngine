#pragma once
#include <cstdint>
#include <string>

using AssetID = uint64_t;

enum class AssetType : uint32_t
{
    Unknown = 0,
    Texture2D,
    StaticMesh,
    SkeletalMesh,
    Animation,
    Material,
    Shader,
    Scene,
    Folder // 편의상(폴더도 아이템처럼 다루고 싶으면)
};

inline const char* AssetTypeToString(AssetType t)
{
    switch (t)
    {
    case AssetType::Texture2D:  return "Texture2D";
    case AssetType::StaticMesh: return "StaticMesh";
    case AssetType::SkeletalMesh: return "SkeletalMesh";
    case AssetType::Animation:  return "Animation";
    case AssetType::Material:   return "Material";
    case AssetType::Shader:     return "Shader";
    case AssetType::Scene:      return "Scene";
    case AssetType::Folder:     return "Folder";
    default:                    return "Unknown";
    }
}

inline AssetType AssetTypeFromString(const std::string& s)
{
    if (s == "Texture2D")   return AssetType::Texture2D;
    if (s == "StaticMesh")  return AssetType::StaticMesh;
    if (s == "SkeletalMesh") return AssetType::SkeletalMesh;
    if (s == "Animation")   return AssetType::Animation;
    if (s == "Material")    return AssetType::Material;
    if (s == "Shader")      return AssetType::Shader;
    if (s == "Scene")       return AssetType::Scene;
    if (s == "Folder")      return AssetType::Folder;
    return AssetType::Unknown;
}
