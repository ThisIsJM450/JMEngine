#pragma once

#include <memory>
#include <vector>

#include "SceneView.h"
#include "RenderQueue.h"
#include "RenderTypes.h"
#include "FrameResources.h"
#include "Pass/ShadowPass.h"
#include "Pass/ForwardPass.h"
#include "Pass/CubeMapPass.h"
#include "Pass/ToneMapPass.h"
#include "../Scene/Scene.h"

class Dx11Context;
class Scene;

class Renderer
{
public:
    void Create(Dx11Context& gfx);
    ~Renderer() {}

    SceneView BuildSceneView(Dx11Context& gfx, const Scene& scene);
    void SetMainViewport(ID3D11DeviceContext* ctx, int w, int h);

    void RenderRuntimeScene(Dx11Context& gfx, const Scene& scene);
    void CompositeToBackBuffer(Dx11Context& gfx);

    FrameResources& GetFrameResources() { return m_Frame; }
    const RenderQueue& GetRenderQueue() const { return m_Queue; }
    const SceneView& GetSceneView() const { return m_LastSceneView; }
    const ShadowOutput& GetShadowOutput() const { return m_ShadowOut; }

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

    ShadowOutput m_ShadowOut;
    SceneView m_LastSceneView;
};
