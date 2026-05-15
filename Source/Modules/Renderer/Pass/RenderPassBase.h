#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cassert>

class Dx11Context;

// 패스가 출력할 타겟/클리어/뷰포트 설정
struct PassBeginDesc
{
    // OM targets
    ID3D11RenderTargetView* rtv = nullptr;   // 색 출력(없으면 nullptr)
    ID3D11DepthStencilView* dsv = nullptr;   // 깊이(없으면 nullptr)

    // Clear
    bool clearColor = false;
    float clearRGBA[4] = { 0,0,0,0 };

    bool clearDepth = false;
    float clearDepthValue = 1.0f;
    bool clearStencil = false;
    UINT8 clearStencilValue = 0;

    // Viewport
    bool setViewport = false;
    float viewportW = 0;
    float viewportH = 0;
};

class RenderPassBase
{
public:
    virtual ~RenderPassBase() = default;
    virtual void Create(Dx11Context& gfx) {}
    virtual void Execute(Dx11Context& gfx, ID3D11ShaderResourceView* sceneColorHDR) {}

protected:
    // BeginPass: 모든 패스는 Execute 시작에서 이것만 호출해서 타겟/클리어/뷰포트 통일
    void BeginPass(ID3D11DeviceContext* ctx, const PassBeginDesc& desc);

    // EndPass: 기본은 noop(필요하면 자식이 직접 후처리)
    void EndPass(ID3D11DeviceContext* /*ctx*/) {}
};
