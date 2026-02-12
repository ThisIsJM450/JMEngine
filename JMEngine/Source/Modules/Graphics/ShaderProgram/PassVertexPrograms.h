#pragma once
#include <memory>
#include <d3d11.h>

#include "SkinnedVertexProgram.h"
#include "StaticVertexProgram.h"

struct PassVertexPrograms
{
    std::unique_ptr<StaticVertexProgram>  StaticVP;
    std::unique_ptr<SkinnedVertexProgram> SkinnedVP;
    bool bInited = false;

    void Ensure(ID3D11Device* dev, const wchar_t* staticVS, const wchar_t* skinnedVS)
    {
        if (bInited) return;

        StaticVP = std::make_unique<StaticVertexProgram>();
        SkinnedVP = std::make_unique<SkinnedVertexProgram>();

        StaticVP->Create(dev, staticVS);
        SkinnedVP->Create(dev, skinnedVS);

        bInited = true;
    }

    void Bind(ID3D11DeviceContext* ctx, bool bSkinned) const
    {
        if (!bInited) return;

        if (bSkinned)
        {
            ctx->IASetInputLayout(SkinnedVP->GetIL());
            ctx->VSSetShader(SkinnedVP->GetVS(), nullptr, 0);
        }
        else
        {
            ctx->IASetInputLayout(StaticVP->GetIL());
            ctx->VSSetShader(StaticVP->GetVS(), nullptr, 0);
        }
    }
};
