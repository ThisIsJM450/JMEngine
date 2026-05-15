#pragma once

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <iostream>
#include <string>
#include <DirectXMath.h>

#include "../../Game/MeshData.h"

class MeshLoader
{
public:
    void Load(std::string basePath, std::string filename);
    std::vector<MeshAsset> meshes;

private:
    void ProcessNode(aiNode *node, const aiScene *scene, DirectX::XMMATRIX tr);

    MeshAsset ProcessMesh(aiMesh *mesh, const aiScene *scene);

    std::string basePath;

};
