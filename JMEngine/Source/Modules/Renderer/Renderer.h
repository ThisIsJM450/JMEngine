#pragma once

#include <memory>
#include <vector>
#include "SceneView.h"
#include "RenderQueue.h"
#include "RenderTypes.h"
#include "FrameResources.h"
#include "Pass/ShadowPass.h"
#include "Pass/ForwardPass.h"
#include "../Scene/Scene.h"
#include "Pass/CubeMapPass.h"
#include "Pass/DebugPass.h"
#include "Pass/ToneMapPass.h"


class Dx11Context;
class Scene;

class Renderer
{
public:
    void Create(Dx11Context& gfx);
    ~Renderer() {}
    
     SceneView BuildSceneView(Dx11Context& gfx, const Scene& scene);
    void SetMainViewport(ID3D11DeviceContext* ctx, int w, int h);

    void Render(Dx11Context& gfx, const Scene& scene);
    
    const ShadowOutput& GetShadowOutput() { return m_ShadowOut; }

private:
    void BuildRenderQueue(Dx11Context& gfx, const Scene& scene, const SceneView& view, RenderQueue& outQueue);
    void GatherLights(const Scene& scene, std::vector<DirectionalLight>& outDir, std::vector<SpotLight>& outSpot);

private:
    FrameResources m_Frame;
    RenderQueue m_Queue;

    ShadowPass m_ShadowPass;
    ForwardPass m_ForwardPass;
    ToneMapPass m_ToneMapPass;
    CubeMapPass m_CubeMapPass;
    
    DebugPass m_DebugPass;

    ShadowOutput m_ShadowOut;
};