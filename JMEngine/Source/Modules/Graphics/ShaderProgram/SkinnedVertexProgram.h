#pragma once
#include "VertexProgram.h"

class SkinnedVertexProgram : public VertexProgram
{
public:
    void Create(ID3D11Device* device, const wchar_t* vsPath) override;
};
