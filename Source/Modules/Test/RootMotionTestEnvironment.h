#pragma once

#include <DirectXMath.h>
#include <string>

class World;
class Character;

namespace Test
{
    struct RootMotionPairSetupOptions
    {
        std::string OnActorName = "RM_Test_On";
        std::string OffActorName = "RM_Test_Off";

        std::string MeshVirtualPath = "/Game/Mesh/passive_marker_man";
        std::string AnimVirtualPath = "/Game/Animation/Capoeira";
        std::string MeshSearchText = "passive_marker_man";
        std::string AnimSearchText = "capoeira";

        bool bUseCustomActorBaseTransform = true;
        DirectX::XMFLOAT3 ActorBasePos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 ActorBaseRotEulerRad{1.6f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 ActorBaseScale{0.02f, 0.02f, 0.02f};
        float PairSpacingX = 4.0f;

        DirectX::XMFLOAT3 MeshRelativePos{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 MeshRelativeRotEulerRad{0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 MeshRelativeScale{1.0f, 1.0f, 1.0f};

        std::string SectionName = "Default";
        bool bLoop = true;
        float PlayRate = 1.0f;
    };

    struct RootMotionPairSetupResult
    {
        bool bSuccess = false;
        std::string Message;
        Character* OnActor = nullptr;
        Character* OffActor = nullptr;
    };

    RootMotionPairSetupResult SetupRootMotionOnOffPairInWorld(
        World& world,
        const RootMotionPairSetupOptions& options = RootMotionPairSetupOptions());
}
