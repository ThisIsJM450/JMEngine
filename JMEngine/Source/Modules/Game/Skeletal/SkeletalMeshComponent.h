#pragma once
#include "../Components/MeshComponent.h"

struct Skeleton;
class AnimInstance;
class SkeletalMeshAsset;
class SkeletalMeshSceneProxy; // 새로 만들 프록시

class SkeletalMeshComponent : public MeshComponent
{
public:
    SkeletalMeshComponent();

    void SetMesh(const std::shared_ptr<SkeletalMeshAsset>& mesh);
    const SkeletalMeshAsset* GetMesh() const { return m_Mesh.get(); }
    Skeleton* GetSkeleton() const;
    AnimInstance* GetAnimInstance() const { return m_Anim.get(); }
    
    // 애니메이션 평가 후 bone palette 갱신 같은 건 여기서 수행
    void Tick(float deltaTime) override;

protected:

    std::unique_ptr<SceneProxyBase> CreateSceneProxy() override;
    void FillProxyCommonData(SceneProxyBase* proxy) override;
    void FillProxyMeshData(SceneProxyBase* proxy) override;

private:
    void PushBonePaletteToProxy();
    
    void BuildFinalPalette_FromLocalPose(const std::vector<DirectX::XMFLOAT4X4>& localPose);
    void BuildGlobalPose_FromLocalPose(const std::vector<DirectX::XMFLOAT4X4>& localPose);

private:
    std::shared_ptr<SkeletalMeshAsset> m_Mesh = nullptr;
    
    std::unique_ptr<AnimInstance> m_Anim;

    std::vector<DirectX::XMFLOAT4X4> m_GlobalPose;
    std::vector<DirectX::XMFLOAT4X4> m_FinalPalette; // 런타임 Pallette값
    
    std::vector<Transform> RefPose;

};
