#include "MeshLoader.h"
#include <filesystem>
#include <assimp\postprocess.h>

using namespace DirectX;

void MeshLoader::Load(std::string basePath, std::string filename)
{
    this->basePath = basePath;

    Assimp::Importer importer;

    const aiScene *pScene = importer.ReadFile(
        this->basePath + filename,
        aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);

    XMMATRIX tr = XMMATRIX();
    ProcessNode(pScene->mRootNode, pScene, tr);
}

void MeshLoader::ProcessNode(aiNode* node, const aiScene* scene, DirectX::XMMATRIX tr)
{
    // node local transform (Assimp) -> XMMATRIX
    const aiMatrix4x4& a = node->mTransformation;
    XMMATRIX m = XMMATRIX(
        a.a1, a.a2, a.a3, a.a4,
        a.b1, a.b2, a.b3, a.b4,
        a.c1, a.c2, a.c3, a.c4,
        a.d1, a.d2, a.d3, a.d4
    );
    const XMMATRIX local = XMMatrixTranspose(m);

    // 기존 로직: m = m.Transpose() * tr;
    // 동일 의미: world = local(transposed) * parent
    const XMMATRIX world = XMMatrixMultiply(local, tr);

    for (int32_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        auto newMesh = this->ProcessMesh(mesh, scene);

        // vertices position transform (Vec3)
        for (auto& v : newMesh.Vertices)
        {
            // v.position이 SimpleMath::Vector3 같은 타입이면 x,y,z로 접근 가능하다고 가정
            XMVECTOR p = XMVectorSet(v.Pos.x, v.Pos.y, v.Pos.z, 1.0f);

            // 좌표(포지션) 변환은 coord 사용(마지막 w로 나눔 포함)
            XMVECTOR tp = XMVector3TransformCoord(p, world);

            XMFLOAT3 out;
            XMStoreFloat3(&out, tp);

            v.Pos.x = out.x;
            v.Pos.y = out.y;
            v.Pos.z = out.z;
        }

        meshes.push_back(std::move(newMesh));
    }

    for (int32_t i = 0; i < node->mNumChildren; ++i)
    {
        this->ProcessNode(node->mChildren[i], scene, world);
    }
}

MeshAsset MeshLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    // Data to fill
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Walk through each of the mesh's vertices
    for (int32_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        vertex.Pos.x = mesh->mVertices[i].x;
        vertex.Pos.y = mesh->mVertices[i].y;
        vertex.Pos.z = mesh->mVertices[i].z;

        vertex.Normal.x = mesh->mNormals[i].x;
        vertex.Normal.y = mesh->mNormals[i].y;
        vertex.Normal.z = mesh->mNormals[i].z;
        XMFLOAT3 NormalVec;
        XMStoreFloat3(&NormalVec, XMVector3Normalize(XMLoadFloat3(&vertex.Normal)));     
        vertex.Normal = NormalVec;

        if (mesh->mTextureCoords[0]) {
            vertex.UV.x = (float)mesh->mTextureCoords[0][i].x;
            vertex.UV.y = (float)mesh->mTextureCoords[0][i].y;
        }

        vertices.push_back(vertex);
    }

    for (int32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (int32_t j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    MeshAsset newMesh;
    newMesh.Vertices = vertices;
    newMesh.Indices = indices;
    
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString filepath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &filepath);

            std::string fullPath =
                this->basePath +
                std::string(std::filesystem::path(filepath.C_Str())
                                .filename()
                                .string());

            //newMesh.textureFilename = fullPath;
        }
    }

    return newMesh;
}
