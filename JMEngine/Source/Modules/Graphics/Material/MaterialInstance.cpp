#include "MaterialInstance.h"

#include "../../Renderer/RenderTypes.h"

void MaterialInstance::Bind(ID3D11Device* Dev, ID3D11DeviceContext* Context, PassType Pass) const
{
    // 1) 파이프라인 바인딩
    if (m_Base)
    {
        m_Base->Bind(Context, Pass);
    }

    // 2) 파라미터 바인딩 (material CB = b4)
    // Shadow 패스는 보통 텍스처/PS 필요 없으니 생략
    if (Pass == PassType::Forward)
    {
        m_Params.Bind(Dev, Context, /*b*/5, /*t*/0, /*s*/3);
    }
}
