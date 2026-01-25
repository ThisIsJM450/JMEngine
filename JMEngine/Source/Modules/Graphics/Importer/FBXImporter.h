#pragma once
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>

struct aiScene;
struct MeshAsset;
class MaterialInstance;

struct ImportOptions
{
    bool bFlipUV = true;
    bool bConvertToLeftHanded = true;
    bool bGenerateNormalsIfMissing = true;
    bool bGenerateTangents = false; // Vertex에 tangent 없으면 false 유지
    
    bool bMergeAllMeshes = true;
};

struct ImportResult
{
    std::shared_ptr<MeshAsset> Mesh;
    // std::shared_ptr<MaterialInstance> Material;
    std::vector<std::shared_ptr<MaterialInstance>> Materials;
};


class FbxImporter
{
public:
    static bool ImportFBX(const std::string& path, const ImportOptions& opt, ImportResult& out);

private:
    static std::shared_ptr<MeshAsset> BuildMeshAssetFromScene(const aiScene* scene, const ImportOptions& opt, int& outPickedMaterialIndex);
    static std::vector<std::shared_ptr<MaterialInstance>> BuildMaterialsByMatIndex(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir, const std::vector<uint8_t>& usedMaterials);
};
