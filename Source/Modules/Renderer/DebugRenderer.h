#pragma once

class Dx11Context;
class Renderer;
class Scene;

class DebugRenderer
{
public:
    void Render(Dx11Context& gfx, Renderer& renderer, const Scene& scene);
};
