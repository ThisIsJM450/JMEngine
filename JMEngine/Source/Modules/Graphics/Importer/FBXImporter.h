#pragma once
#include "FBXImportType.h"

class FbxImporter
{
public:
    static bool ImportFBX(const std::string& path, const ImportOptions& opt, ImportResult& out);

private:
    static bool BuildStatic(
        const struct aiScene* scene,
        const ImportOptions& opt,
        std::shared_ptr<struct MeshAsset>& outMesh,
        std::vector<uint8_t>& outUsedMaterials);

    static bool BuildSkeletal(
        const struct aiScene* scene,
        const ImportOptions& opt,
        std::shared_ptr<SkeletalMeshAsset>& outMesh,
        std::vector<uint8_t>& outUsedMaterials);
};
