#pragma once

#include "../RenderTypes.h"
#include "ShadowPass.h"
#include "../Material/MaterialInstanceGPUManager.h"

struct RenderQueue;
class Dx11Context;
class FrameResources;
class SceneView;

class ForwardPass
{
public:
    void Execute(Dx11Context& gfx, FrameResources& fr,
        const SceneView& view,
        const RenderQueue& queue,
        const std::vector<DirectionalLight>& dirLights,
        const std::vector<SpotLight>& SpotLights,
        const ShadowOutput& shadows);

    //MaterialInstanceGPUManager materialInstanceGPUManager;
};
