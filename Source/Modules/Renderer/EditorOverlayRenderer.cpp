#include "EditorOverlayRenderer.h"

#include "Renderer.h"
#include "../Editor/EditorContext.h"
#include "../Game/Actor.h"
#include "../Game/Components/StaticMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/Skeletal/SkeletalMeshSceneProxy.h"
#include "../Renderer/SceneProxy/StaticMeshSceneProxy.h"
#include "../Scene/Scene.h"

namespace
{
    const Actor* ResolveOwnerActor(const SceneProxyBase* proxy)
    {
        if (proxy == nullptr)
        {
            return nullptr;
        }

        if (const auto* smProxy = dynamic_cast<const StaticMeshSceneProxy*>(proxy))
        {
            if (smProxy->Ownner)
            {
                return smProxy->Ownner->GetOwner();
            }
        }
        else if (const auto* skProxy = dynamic_cast<const SkeletalMeshSceneProxy*>(proxy))
        {
            if (skProxy->Ownner)
            {
                return skProxy->Ownner->GetOwner();
            }
        }

        return nullptr;
    }
}

void EditorOverlayRenderer::Render(Dx11Context& gfx, Renderer& renderer, const Scene& scene)
{
    (void)gfx;
    (void)renderer;

    m_SelectedItemCount = 0;

    const std::shared_ptr<Actor> selectedActor = GEditor.selection.SelectedActor.lock();
    if (!selectedActor)
    {
        return;
    }

    const Actor* selectedActorPtr = selectedActor.get();
    for (const SceneProxyBase* proxy : scene.GetMesheProxies())
    {
        if (ResolveOwnerActor(proxy) == selectedActorPtr)
        {
            ++m_SelectedItemCount;
        }
    }

    // Groundwork pass only for now:
    // editor-facing selection/highlight ownership lives here instead of Renderer.
    // Actual overlay drawing can later consume m_SelectedItemCount / selected proxy gathering.
}
