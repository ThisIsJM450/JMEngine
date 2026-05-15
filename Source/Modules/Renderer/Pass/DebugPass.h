#pragma once

class FrameResources;
class Dx11Context;
struct SceneView;
struct RenderQueue;

class DebugPass
{
public:
    void Execute(Dx11Context& gfx, FrameResources& fr, const SceneView& view, const RenderQueue& queue);
    
};
