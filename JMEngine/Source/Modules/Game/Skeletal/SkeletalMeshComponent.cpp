#include "SkeletalMeshComponent.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>

#include "SkeletalMeshData.h"
#include "SkeletalMeshSceneProxy.h"
#include "../World.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../Animation/AnimInstance.h"

SkeletalMeshComponent::SkeletalMeshComponent()
{
    TypeName = std::string("SkeletalMeshComponent");
}

void SkeletalMeshComponent::SetMesh(const std::shared_ptr<SkeletalMeshAsset>& mesh)
{
    m_Mesh = mesh;
    
    if (m_Mesh)
    {
        const size_t n = m_Mesh->Skeleton.Bones.size();
        m_GlobalPose.resize(n);
        m_FinalPalette.resize(n);

        m_Anim = std::make_unique<AnimInstance>();
        m_Anim->Initialize(m_Mesh);

        // 초기값은 bind/ref로
        m_FinalPalette = m_Mesh->BonePalette; 
    }
    
    MarkRenderStateDirty();
}

Skeleton* SkeletalMeshComponent::GetSkeleton() const
{
    if (m_Mesh)
    {
        return &m_Mesh->Skeleton;
    }
    
    return nullptr;
}

void SkeletalMeshComponent::Tick(float dt)
{
    MeshComponent::Tick(dt);
    if (!GetMesh() || !m_Anim) return;

    m_Anim->Tick(dt);
    
    BuildFinalPalette_FromLocalPose(m_Anim->GetLocalPose()); 
    PushBonePaletteToProxy();

}

std::unique_ptr<SceneProxyBase> SkeletalMeshComponent::CreateSceneProxy()
{
    if (!GetMesh())
        return nullptr;

    return std::unique_ptr<SceneProxyBase>(new SkeletalMeshSceneProxy());
}

void SkeletalMeshComponent::FillProxyCommonData(SceneProxyBase* proxy)
{
    SkeletalMeshSceneProxy* p = static_cast<SkeletalMeshSceneProxy*>(proxy);
    p->CastShadow = CastShadow;
    p->ReceiveShadow = ReceiveShadow;
    p->MeshID = meshID;
    p->materialInstances = GetMaterialInstances();
}

void SkeletalMeshComponent::FillProxyMeshData(SceneProxyBase* proxy)
{
    if (!GetMesh())
        return;

    SkeletalMeshSceneProxy* p = static_cast<SkeletalMeshSceneProxy*>(proxy);
    p->Ownner = this;
    p->Mesh = GetMesh();
    p->BonePalette = m_FinalPalette;
}

void SkeletalMeshComponent::PushBonePaletteToProxy()
{
    if (!m_Proxy || !GetMesh())
    return;

    SkeletalMeshSceneProxy* p = static_cast<SkeletalMeshSceneProxy*>(m_Proxy.get());
    p->BonePalette = m_FinalPalette; // 복사
    p->bBoneDirty = true; 
}

static void PrintHeader(int i, const char* boneName = nullptr)
{
    std::cout << "\n==============================\n";
    std::cout << "Bone[" << i << "]";
    if (boneName) std::cout << " (" << boneName << ")";
    std::cout << "\n==============================\n";
}
static void PrintXMF4x4(const char* name, const DirectX::XMFLOAT4X4& m)
{
    std::cout << name << " =\n";
    std::cout << std::fixed << std::setprecision(6);

    std::cout << "[" << std::setw(12) << m._11 << " " << std::setw(12) << m._12 << " " << std::setw(12) << m._13 << " " << std::setw(12) << m._14 << "]\n";
    std::cout << "[" << std::setw(12) << m._21 << " " << std::setw(12) << m._22 << " " << std::setw(12) << m._23 << " " << std::setw(12) << m._24 << "]\n";
    std::cout << "[" << std::setw(12) << m._31 << " " << std::setw(12) << m._32 << " " << std::setw(12) << m._33 << " " << std::setw(12) << m._34 << "]\n";
    std::cout << "[" << std::setw(12) << m._41 << " " << std::setw(12) << m._42 << " " << std::setw(12) << m._43 << " " << std::setw(12) << m._44 << "]\n";

    std::cout << std::defaultfloat;
}

void SkeletalMeshComponent::BuildFinalPalette_FromLocalPose(const std::vector<DirectX::XMFLOAT4X4>& localPose)
{
    auto MulM = [](const DirectX::XMFLOAT4X4& A, const DirectX::XMFLOAT4X4& B)
    {
        using namespace DirectX;
        XMMATRIX a = XMLoadFloat4x4(&A);
        XMMATRIX b = XMLoadFloat4x4(&B);
        XMFLOAT4X4 out;
        XMStoreFloat4x4(&out, a * b);
        return out;
    };
    const Skeleton& skel = GetMesh()->Skeleton;
    const uint32_t n = (uint32_t)skel.Bones.size();
    if ((uint32_t)localPose.size() != n) return;


    m_GlobalPose.resize(n);
    m_FinalPalette.resize(n);

    BuildGlobalPose_FromLocalPose(localPose);

    // FinalPalette
    for (uint32_t i = 0; i < n; ++i)
    {
        const auto& off = skel.Bones[i].Offset;
        m_FinalPalette[i] = MulM(MulM(skel.GlobalInverse, m_GlobalPose[i]), off);
        XMStoreFloat4x4(&m_FinalPalette[i], XMLoadFloat4x4(&m_FinalPalette[i]));
    }

}

void SkeletalMeshComponent::BuildGlobalPose_FromLocalPose(const std::vector<DirectX::XMFLOAT4X4>& localPose)
{
    using namespace DirectX;

    auto MulM = [](const XMFLOAT4X4& A, const XMFLOAT4X4& B)
    {
        XMMATRIX a = XMLoadFloat4x4(&A);
        XMMATRIX b = XMLoadFloat4x4(&B);
        XMFLOAT4X4 out;
        XMStoreFloat4x4(&out, a * b);
        return out;
    };

    const Skeleton& skel = GetMesh()->Skeleton;
    const auto& Bones = skel.Bones;
    const auto& Nodes = skel.Nodes;

    if (Bones.empty() || Nodes.empty())
        return;

    if (localPose.size() < Bones.size())
    {
        std::cout << "localPose size mismatch\n";
        return;
    }

    // 1) nodeLocal: RefLocal로 시작 
    std::vector<XMFLOAT4X4> nodeLocal(Nodes.size());
    for (size_t ni = 0; ni < Nodes.size(); ++ni)
    {
        nodeLocal[ni] = Nodes[ni].RefLocal;
    }
    
    ///임시
    //nodeLocal = m_Anim->GetAnimRefPose();
    ///임시

    // 2) bone local을 해당 bone node에 덮어쓰기
    for (uint32_t bi = 0; bi < (uint32_t)Bones.size(); ++bi)
    {
        const int32_t nodeIdx = (bi < skel.BoneToNode.size()) ? skel.BoneToNode[bi] : -1;
        if (nodeIdx < 0 || nodeIdx >= (int32_t)Nodes.size())
            continue;

        nodeLocal[nodeIdx] = localPose[bi];
    }

    // 3) nodeGlobal: 여기서만 임시로 만든다 (저장 X)
    std::vector<XMFLOAT4X4> nodeGlobal(Nodes.size());
    std::vector<uint8_t> visited(Nodes.size(), 0);

    // 루트들부터 BFS (Parent == -1)
    std::queue<int32_t> q;
    for (int32_t i = 0; i < (int32_t)Nodes.size(); ++i)
    {
        if (Nodes[i].Parent < 0)
        {
            nodeGlobal[i] = nodeLocal[i]; // root global = root local (Assimp와 동일)
            visited[i] = 1;
            q.push(i);
        }
    }

    while (!q.empty())
    {
        int32_t cur = q.front(); q.pop();

        for (int32_t c : Nodes[cur].Children)
        {
            nodeGlobal[c] = MulM(nodeGlobal[cur], nodeLocal[c]);
            visited[c] = 1;
            q.push(c);
        }
    }

    // 4) bone global pose는 nodeGlobal에서 뽑아 채움
    m_GlobalPose.resize(Bones.size());

    for (uint32_t bi = 0; bi < (uint32_t)Bones.size(); ++bi)
    {
        const int32_t nodeIdx = (bi < skel.BoneToNode.size()) ? skel.BoneToNode[bi] : -1;
        if (nodeIdx < 0 || nodeIdx >= (int32_t)Nodes.size())
            continue;

        if (!visited[nodeIdx]) continue;
        m_GlobalPose[bi] = nodeGlobal[nodeIdx];
    }
}
