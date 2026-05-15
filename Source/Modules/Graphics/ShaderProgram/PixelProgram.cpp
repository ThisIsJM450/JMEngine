#include "PixelProgram.h"
#include "ShaderCompileUtils.h"

void PixelProgram::Create(ID3D11Device* device, const wchar_t* psPath, const wchar_t* gsPath)
{
    if (psPath)
    {
        auto ps = ShaderCompileUtils::Compile(psPath, "PSMain", "ps_5_0");
        if (ps)
            device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_PS);
    }
    else
    {
        m_PS.Reset();
    }

    if (gsPath)
    {
        auto gs = ShaderCompileUtils::Compile(gsPath, "GSMain", "gs_5_0");
        if (gs)
            device->CreateGeometryShader(gs->GetBufferPointer(), gs->GetBufferSize(), nullptr, &m_GS);
    }
    else
    {
        m_GS.Reset();
    }
}
