#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>


class my_class
{
public:
    
};

class SkeletalMeshAsset;
struct MeshSection;
class MeshAsset;
struct MaterialInstance;

struct ImportOptions
{
    bool bGenerateNormalsIfMissing = true;
    bool bGenerateTangents = true;
    bool bFlipUV = false;
    bool bConvertToLeftHanded = true;

    // 기존 static 빌드에서만 의미 있음(너 코드 유지)
    bool bMergeAllMeshes = true;
};

enum class EImportedMeshType
{
    Static,
    Skeletal
};


// -----------------------------
// Unified import result (static/skeletal 분리 보관)
// -----------------------------
struct ImportResult
{
    EImportedMeshType Type = EImportedMeshType::Static;

    std::shared_ptr<MeshAsset> StaticMesh;
    std::shared_ptr<SkeletalMeshAsset> SkeletalMesh;

    std::vector<std::shared_ptr<MaterialInstance>> Materials;
};
