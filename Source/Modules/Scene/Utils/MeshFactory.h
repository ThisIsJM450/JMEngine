#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "../../Game/MeshData.h"

class Dx11Context;

/*
class MeshFactory
{
public:
    static MeshEntity CreatePlane(Dx11Context& gfx, float halfSize);
    static MeshEntity CreateCube(Dx11Context& gfx, float half);
    static MeshEntity CreateSphere(Dx11Context& gfx, float radius, uint32_t stacks, uint32_t slices);

private:
    static MeshEntity CreateMeshEntity(Dx11Context& gfx,const std::vector<Vertex> vertices, std::vector<uint16_t> indexs);
    static MeshAsset CreateMeshData(Dx11Context& gfx, const std::vector<Vertex> vertices, std::vector<uint16_t> indexs);
};
*/
class MeshFactory
{
public:
    static std::shared_ptr<MeshAsset> CreatePlane(float halfSize);
    static std::shared_ptr<MeshAsset> CreateCube(float half);
    static std::shared_ptr<MeshAsset> CreateSphere(float radius, uint32_t stacks, uint32_t slices);
    

private:
    static void BuildTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
};