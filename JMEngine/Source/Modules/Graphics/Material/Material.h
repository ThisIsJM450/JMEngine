#pragma once
#include <d3d11.h>
#include <memory>
#include <string>

#include "../RenderState.h"
#include "../ShaderProgram.h"


enum class PassType : unsigned char;

class Material
{
public:
    Material(const wchar_t* InvsPath, const wchar_t* InpsPath) 
        : vsPath(InvsPath), psPath(InpsPath), bDirtyState(true) {gsPath = nullptr;}
    Material(const wchar_t* InvsPath, const wchar_t* InpsPath, const wchar_t* IngsPath) 
        : vsPath(InvsPath), psPath(InpsPath), gsPath(IngsPath), bDirtyState(true) {}

    // void Create(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath, const RenderStateDesc& rsDesc);
    // void Bind(ID3D11DeviceContext* context) const;

    void Bind(ID3D11DeviceContext* ctx, PassType pass);
    
    // ID3D11InputLayout* GetInputLayout() const { return m_Shader.GetIL(); }
    void SetCullBack(bool cullBack) { bCullBack = cullBack; bDirtyState = true; }
    void SetDepthEnable(bool depthEnable) { bDepthEnable = depthEnable; bDirtyState = true;}
    void SetEnableShadow(bool enableShadow) { bEnableShadow = enableShadow;}
    
    void SetShadowVSPath(const wchar_t* InPath) { vsPath_Shadow = InPath; bDirtyState = true;}
    // void SetShadowPSPath(const std::string& InPath) { psPath_Shadow = InPath; bDirtyState = true;}

private:
    
    void UpdateForwardState(ID3D11Device* dev);
    void UpdateShadowState(ID3D11Device* dev);

    /**
     * Pass마다 sp, rs 분리
     */
    std::shared_ptr<ShaderProgram> m_Forward;
    std::shared_ptr<ShaderProgram> m_Shadow;
    std::shared_ptr<ShaderProgram> m_Debug;

    RenderState m_ForwardState;
    RenderState m_ShadowState;
    RenderState m_DebugState;
    
    bool bCullBack = true;
    bool bDepthEnable = true;
    bool bEnableShadow = true;
    
    const wchar_t* vsPath;
    const wchar_t* psPath;
    const wchar_t* gsPath;
    
    const wchar_t* vsPath_Shadow = nullptr;
    
    bool bDirtyState = false;
};
