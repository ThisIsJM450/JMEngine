#include "MaterialInstanceGPUManager.h"

#include "../../Core/Assert/CoreAssert.h"

void MaterialInstanceGPUManager::Bind(ID3D11Device* dev, ID3D11DeviceContext* ctx, const MaterialInstance& mi, PassType Pass)
{
    mi.Bind(dev, ctx, Pass);
    MaterialInstanceGPUData& gpu = GetOrCreate(dev, mi);

    /*
    // b4 바인딩
    ID3D11Buffer* cb = gpu.CBMaterial.Get();
    ctx->VSSetConstantBuffers(4, 1, &cb);
    ctx->PSSetConstantBuffers(4, 1, &cb);

    // 텍스처/샘플러 
    ID3D11ShaderResourceView* srvs[8]{};
    ctx->PSSetShaderResources(0, 8, srvs);

    ID3D11SamplerState* samps[8]{};
    ctx->PSSetSamplers(0, 8, samps);
    */
}

MaterialInstanceGPUData& MaterialInstanceGPUManager::GetOrCreate(ID3D11Device* dev, const MaterialInstance& mi)
{
    // 키는 “MaterialInstance 객체 주소”로 충분 (수명은 게임/월드가 관리)
    auto key = reinterpret_cast<uintptr_t>(&mi);
    static std::unordered_map<uintptr_t, MaterialInstanceGPUData> s_cache;

    auto it = s_cache.find(key);
    if (it != s_cache.end())
        return it->second;

    MaterialInstanceGPUData gpu{};

    D3D11_BUFFER_DESC desc{};
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = (UINT)sizeof(CBMaterial);

    CoreAssert::CheckHR(dev->CreateBuffer(&desc, nullptr, gpu.CBMaterial.GetAddressOf()), L"Create CBMaterial failed");
    //gpu.LastVersion = 0;

    auto newIt = s_cache.emplace(key, std::move(gpu));
    return newIt.first->second;
}


