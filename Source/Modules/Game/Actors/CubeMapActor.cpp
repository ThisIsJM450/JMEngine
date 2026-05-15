#include "CubeMapActor.h"

#include "../../Graphics/Material/Material.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Graphics/Texture/Texture.h"
#include "../../Scene/Utils/MeshFactory.h"

CubeMapActor::CubeMapActor()
{
    GetRootComponent()->GetRelativeTransform().SetPosition(0.f, 0.f, 0.f );
    GetRootComponent()->GetRelativeTransform().SetScale(1.f, 1.f,  1.f);
    GetMeshComponent()->SetMesh(MeshFactory::CreateCube(100.f));

    const wchar_t* shaderPath = L"Shader\\ForwardCubeMap.hlsl";
    std::shared_ptr<Material> Mat = std::make_shared<Material>();
    if (Mat)
    {
        Mat->psPath = shaderPath;
        Mat->bCullBack = false;
        Mat->bEnableShadow = false;
        Mat->bDepthEnable = true;
        if (std::shared_ptr<MaterialInstance> MI = std::make_shared<MaterialInstance>(Mat))
        {
            MI->SetTexture(0, TextureFileName::GetTexture_Environment_Moonless());
            MI->SetTexture(1, TextureFileName::GetTexture_Environment_Moonless_Diffuse());
            MI->SetTexture(2, TextureFileName::GetTexture_Environment_Moonless_PrefilterMap());
            MI->SetTexture(3, TextureFileName::GetTexture_Environment_Moonless_LookUpTexture());
            MI->EnableCubeMap(true);
            GetMeshComponent()->SetMaterial({MI});
        }
    }
}
