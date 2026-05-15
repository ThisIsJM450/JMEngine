#include "RootMotionTestEnvironment.h"

#include <sstream>
#include <vector>

#include "../Core/AppBase.h"
#include "../Core/Asset/AssetRegistry.h"
#include "../Game/Actor.h"
#include "../Game/Animation/AnimInstance.h"
#include "../Game/Animation/AnimSequence.h"
#include "../Game/Characters/Character.h"
#include "../Game/Components/CharacterMovementComponenet.h"
#include "../Game/Skeletal/SkeletalMeshComponent.h"
#include "../Game/World.h"
#include "../Graphics/Importer/FBXImporter.h"
#include "../Graphics/Importer/FBXImporter_Animation.h"

namespace
{
    const AssetMeta* FindAssetByVirtualPathOrText(
        const AssetRegistry& registry,
        AssetType type,
        const std::string& exactVirtualPath,
        const std::string& textFallback)
    {
        AssetQuery query{};
        query.type = type;
        query.text = textFallback;
        const std::vector<AssetID> ids = registry.Query(query);

        for (const AssetID id : ids)
        {
            const AssetMeta* meta = registry.GetMeta(id);
            if (meta && meta->virtualPath == exactVirtualPath)
            {
                return meta;
            }
        }

        if (!ids.empty())
        {
            return registry.GetMeta(ids.front());
        }
        return nullptr;
    }

    Character* FindCharacterByName(World& world, const std::string& name)
    {
        for (const std::shared_ptr<Actor>& actor : world.GetActors())
        {
            if (!actor || actor->GetName() != name)
            {
                continue;
            }
            if (Character* character = dynamic_cast<Character*>(actor.get()))
            {
                return character;
            }
        }
        return nullptr;
    }

    Character* FindOrSpawnCharacter(World& world, const std::string& name)
    {
        if (Character* existing = FindCharacterByName(world, name))
        {
            return existing;
        }
        return world.SpawnActor<Character>(name);
    }
}

namespace Test
{
    RootMotionPairSetupResult SetupRootMotionOnOffPairInWorld(
        World& world,
        const RootMotionPairSetupOptions& options)
    {
        RootMotionPairSetupResult result{};

        if (!GAssetRegistry)
        {
            result.Message = "[RootMotionTestSetup][Error] AssetRegistry is null.";
            return result;
        }

        const AssetMeta* meshMeta = FindAssetByVirtualPathOrText(
            *GAssetRegistry,
            AssetType::SkeletalMesh,
            options.MeshVirtualPath,
            options.MeshSearchText);
        const AssetMeta* animMeta = FindAssetByVirtualPathOrText(
            *GAssetRegistry,
            AssetType::Animation,
            options.AnimVirtualPath,
            options.AnimSearchText);
        if (!meshMeta || !animMeta)
        {
            result.Message = "[RootMotionTestSetup][Error] Test assets are missing.";
            return result;
        }

        ImportOptions importOptions{};
        importOptions.bBuildMaterials = false;

        ImportResult importResult{};
        if (!FbxImporter::ImportFBX(meshMeta->sourcePath, importOptions, importResult) ||
            importResult.Type != EImportedMeshType::Skeletal ||
            !importResult.SkeletalMesh)
        {
            result.Message = "[RootMotionTestSetup][Error] Failed to import skeletal mesh.";
            return result;
        }

        Character* actorOn = FindOrSpawnCharacter(world, options.OnActorName);
        Character* actorOff = FindOrSpawnCharacter(world, options.OffActorName);
        if (!actorOn || !actorOff)
        {
            result.Message = "[RootMotionTestSetup][Error] Failed to create test actors.";
            return result;
        }
        if (!actorOn->GetRootComponent() || !actorOff->GetRootComponent())
        {
            result.Message = "[RootMotionTestSetup][Error] RootComponent is null.";
            return result;
        }

        const float halfSpacingX = options.PairSpacingX * 0.5f;
        const DirectX::XMFLOAT3 basePos =
            options.bUseCustomActorBaseTransform
            ? options.ActorBasePos
            : DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
        const DirectX::XMFLOAT3 baseRot =
            options.bUseCustomActorBaseTransform
            ? options.ActorBaseRotEulerRad
            : DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f};
        const DirectX::XMFLOAT3 baseScale =
            options.bUseCustomActorBaseTransform
            ? options.ActorBaseScale
            : DirectX::XMFLOAT3{0.02f, 0.02f, 0.02f};

        actorOn->GetRootComponent()->GetRelativeTransform().SetPosition(basePos.x - halfSpacingX, basePos.y, basePos.z);
        actorOn->GetRootComponent()->GetRelativeTransform().SetRotationEuler(baseRot.x, baseRot.y, baseRot.z);
        actorOn->GetRootComponent()->GetRelativeTransform().SetScale(baseScale.x, baseScale.y, baseScale.z);
        actorOn->GetRootComponent()->MarkDirty();

        actorOff->GetRootComponent()->GetRelativeTransform().SetPosition(basePos.x + halfSpacingX, basePos.y, basePos.z);
        actorOff->GetRootComponent()->GetRelativeTransform().SetRotationEuler(baseRot.x, baseRot.y, baseRot.z);
        actorOff->GetRootComponent()->GetRelativeTransform().SetScale(baseScale.x, baseScale.y, baseScale.z);
        actorOff->GetRootComponent()->MarkDirty();

        SkeletalMeshComponent* skOn = actorOn->GetSkeletalComponent();
        SkeletalMeshComponent* skOff = actorOff->GetSkeletalComponent();
        if (!skOn || !skOff)
        {
            result.Message = "[RootMotionTestSetup][Error] SkeletalMeshComponent is null.";
            return result;
        }

        skOn->SetMesh(importResult.SkeletalMesh);
        skOn->GetRelativeTransform().SetPosition(options.MeshRelativePos.x, options.MeshRelativePos.y, options.MeshRelativePos.z);
        skOn->GetRelativeTransform().SetRotationEuler(
            options.MeshRelativeRotEulerRad.x, options.MeshRelativeRotEulerRad.y, options.MeshRelativeRotEulerRad.z);
        skOn->GetRelativeTransform().SetScale(options.MeshRelativeScale.x, options.MeshRelativeScale.y, options.MeshRelativeScale.z);
        skOn->MarkDirty();

        skOff->SetMesh(importResult.SkeletalMesh);
        skOff->GetRelativeTransform().SetPosition(options.MeshRelativePos.x, options.MeshRelativePos.y, options.MeshRelativePos.z);
        skOff->GetRelativeTransform().SetRotationEuler(
            options.MeshRelativeRotEulerRad.x, options.MeshRelativeRotEulerRad.y, options.MeshRelativeRotEulerRad.z);
        skOff->GetRelativeTransform().SetScale(options.MeshRelativeScale.x, options.MeshRelativeScale.y, options.MeshRelativeScale.z);
        skOff->MarkDirty();

        Skeleton* skeleton = skOn->GetSkeleton();
        if (!skeleton)
        {
            result.Message = "[RootMotionTestSetup][Error] Skeleton is null.";
            return result;
        }

        std::shared_ptr<AnimSequenceAsset> seqBase =
            FBXImporter_Animation::ImportAnimSequence_SectionKeyTransform(animMeta->sourcePath, *skeleton);
        if (!seqBase || seqBase->Sections.empty())
        {
            result.Message = "[RootMotionTestSetup][Error] Failed to import animation sequence.";
            return result;
        }

        std::shared_ptr<AnimSequenceAsset> seqOn = std::make_shared<AnimSequenceAsset>(*seqBase);
        std::shared_ptr<AnimSequenceAsset> seqOff = std::make_shared<AnimSequenceAsset>(*seqBase);
        for (AnimSection& sec : seqOn->Sections)
        {
            sec.bEnableRootMotion = true;
        }
        for (AnimSection& sec : seqOff->Sections)
        {
            sec.bEnableRootMotion = false;
        }

        AnimInstance* animOn = skOn->GetAnimInstance();
        AnimInstance* animOff = skOff->GetAnimInstance();
        if (!animOn || !animOff)
        {
            result.Message = "[RootMotionTestSetup][Error] AnimInstance is null.";
            return result;
        }

        animOn->SetRootMotionExtractionEnabled(true);
        animOn->SetConsumeRootInPose(true);
        animOn->SetSequence(seqOn);
        animOn->Play(options.SectionName, options.bLoop, options.PlayRate);

        animOff->SetRootMotionExtractionEnabled(true);
        animOff->SetConsumeRootInPose(false);
        animOff->SetSequence(seqOff);
        animOff->Play(options.SectionName, options.bLoop, options.PlayRate);

        CharacterMovementComponent* moveOn = actorOn->GetMovementComponent();
        CharacterMovementComponent* moveOff = actorOff->GetMovementComponent();
        if (!moveOn || !moveOff)
        {
            result.Message = "[RootMotionTestSetup][Error] CharacterMovementComponent is null.";
            return result;
        }

        // Force-reset movement state when reusing same actor names across repeated setups.
        moveOn->ForceResetRootMotionState();
        moveOff->ForceResetRootMotionState();
        moveOn->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
        moveOff->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

        const Transform& onRoot = actorOn->GetRootComponent()->GetRelativeTransform();
        const Transform& offRoot = actorOff->GetRootComponent()->GetRelativeTransform();
        const Transform& onMesh = skOn->GetRelativeTransform();
        const Transform& offMesh = skOff->GetRelativeTransform();

        std::ostringstream oss;
        oss << "[RootMotionTestSetup][Success] Actors=("
            << options.OnActorName << "," << options.OffActorName
            << "), Mesh=" << meshMeta->virtualPath
            << ", Anim=" << animMeta->virtualPath
            << ", RootScaleOn=(" << onRoot.m_Scale.x << "," << onRoot.m_Scale.y << "," << onRoot.m_Scale.z << ")"
            << ", RootScaleOff=(" << offRoot.m_Scale.x << "," << offRoot.m_Scale.y << "," << offRoot.m_Scale.z << ")"
            << ", MeshScaleOn=(" << onMesh.m_Scale.x << "," << onMesh.m_Scale.y << "," << onMesh.m_Scale.z << ")"
            << ", MeshScaleOff=(" << offMesh.m_Scale.x << "," << offMesh.m_Scale.y << "," << offMesh.m_Scale.z << ")";

        result.bSuccess = true;
        result.Message = oss.str();
        result.OnActor = actorOn;
        result.OffActor = actorOff;
        return result;
    }
}
