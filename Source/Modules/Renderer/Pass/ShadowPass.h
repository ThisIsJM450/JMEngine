#pragma once

#include <vector>

#include "../FrameResources.h"
#include "../../Graphics/ShadowMap.h"
#include "../RenderTypes.h"
#include "../SceneView.h"
#include "../RenderQueue.h"
#include "../Material/MaterialInstanceGPUManager.h"

class Dx11Context;
class Material;
class FrameResources;

struct ShadowOutput
{
    std::vector<ShadowMap> directionalMaps;
    std::vector<ShadowMap> spotMaps;
};

class ShadowPass
{
public:
    void Create(Dx11Context& gfx, uint32_t dirRes, uint32_t spotRes);

    void Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& renderQueue,
        const std::vector<DirectionalLight>& dirLights, 
        const std::vector<SpotLight>& spotLights,
        ShadowOutput& output);

private:
    uint32_t m_dirRes = 2048;
    uint32_t m_spotRes = 1024;

    void RenderOneShadow(Dx11Context& gfx, FrameResources& fr, const RenderQueue& renderQueue, ShadowMap& sm, const DirectX::XMMATRIX& lightViewProjection);

    //MaterialInstanceGPUManager materialInstanceGPUManager;
};
