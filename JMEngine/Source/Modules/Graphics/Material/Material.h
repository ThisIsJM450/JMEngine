#pragma once
#include <d3d11.h>
#include <memory>
#include <string>

#include "../RenderState.h"
#include "../ShaderProgram.h"


class PixelProgram;
enum class PassType : unsigned char;

class Material
{
public:
    // 기존 함수들 유지
    void Bind(ID3D11DeviceContext* ctx, PassType pass);

public:
    // 기존 경로 멤버는 유지해도 됨(PS/GS만 사용)
    const wchar_t* psPath = nullptr;
    const wchar_t* gsPath = nullptr;

    const wchar_t* psPath_Shadow = nullptr; // 보통 nullptr일 것(Shadow는 PS 안 씀)

    bool bEnableShadow = true;
    bool bCullBack = true;
    bool bDepthEnable = true;

private:
    void UpdateForwardState(ID3D11Device* dev);
    void UpdateShadowState(ID3D11Device* dev);

private:
    bool bDirtyState = true;

    RenderState m_ForwardState;
    RenderState m_ShadowState;
    RenderState m_DebugState;
    
    
    std::shared_ptr<PixelProgram> m_ForwardPS;
    std::shared_ptr<PixelProgram> m_DebugPS;
};
