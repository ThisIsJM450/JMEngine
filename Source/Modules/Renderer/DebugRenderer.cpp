#include "DebugRenderer.h"

#include "Renderer.h"
#include "Pass/DebugPass.h"
#include "SceneView.h"
#include "RenderQueue.h"
#include "../Scene/Scene.h"
#include "../Graphics/Dx11Context.h"
#include "../Core/Settings/RenderSettings.h"

void DebugRenderer::Render(Dx11Context& gfx, Renderer& renderer, const Scene& scene)
{
    if (!RenderSettings::Get().drawNormalVector)
    {
        return;
    }

    DebugPass debugPass;
    debugPass.Execute(gfx, renderer.GetFrameResources(), renderer.GetSceneView(), renderer.GetRenderQueue());
}
