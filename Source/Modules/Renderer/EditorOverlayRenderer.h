#pragma once

#include <cstddef>

class Dx11Context;
class Renderer;
class Scene;

class EditorOverlayRenderer
{
public:
    void Render(Dx11Context& gfx, Renderer& renderer, const Scene& scene);

    size_t GetSelectedItemCount() const { return m_SelectedItemCount; }

private:
    size_t m_SelectedItemCount = 0;
};
