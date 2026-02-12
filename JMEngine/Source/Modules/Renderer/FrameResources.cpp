#include "FrameResources.h"

#include <iosfwd>
#include <string>

void CheckHR(HRESULT hr)
{
    if (SUCCEEDED(hr)) return;

    wchar_t* buf = nullptr;

    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS;

    DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

    DWORD size = FormatMessageW(
        flags,
        nullptr,
        (DWORD)hr,
        langId,
        (LPWSTR)&buf,
        0,
        nullptr
    );

    std::wstring msg;
    if (size && buf)
    {
        msg.assign(buf, buf + size);
        LocalFree(buf);
    }
    else
    {
        wchar_t tmp[64];
        swprintf_s(tmp, L"(No system message) hr=0x%08X", (unsigned)hr);
        msg = tmp;
    }

    OutputDebugStringW(msg.c_str());

    assert(false);

    // 디버그에서 바로 멈추고 싶으면:
    __debugbreak();
}

static void CreateDynamicCB(ID3D11Device* device, UINT byteWidth, ID3D11Buffer** outBuffer)
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (byteWidth + 15u) & ~ 15u;
    bd.Usage = D3D11_USAGE_DYNAMIC ;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = device->CreateBuffer(&bd, nullptr, outBuffer);
    assert(SUCCEEDED(hr));
}

static void UpdateCB(ID3D11DeviceContext* context, ID3D11Buffer* cb, const void* data, size_t size)
{
    D3D11_MAPPED_SUBRESOURCE ms{};
    HRESULT hr = context->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    assert(SUCCEEDED(hr));
    memcpy(ms.pData, data, size);
    context->Unmap(cb, 0);
}

static void CreateDynamicStructuredBuffer(ID3D11Device* device, UINT byteWidth, UINT stride, ID3D11Buffer** outBuffer)
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = byteWidth;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = stride;

    HRESULT hr = device->CreateBuffer(&bd, nullptr, outBuffer);
    assert(SUCCEEDED(hr));
}

static void CreateStructuredBufferSRV(ID3D11Device* device, ID3D11Buffer* sb, UINT numElements, ID3D11ShaderResourceView** outSRV)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sd.Format = DXGI_FORMAT_UNKNOWN; // structured buffer
    sd.Buffer.FirstElement = 0;
    sd.Buffer.NumElements = numElements;

    HRESULT hr = device->CreateShaderResourceView(sb, &sd, outSRV);
    assert(SUCCEEDED(hr));
}

static void UpdateSB(ID3D11DeviceContext* context, ID3D11Buffer* sb, const void* data, size_t size)
{
    D3D11_MAPPED_SUBRESOURCE ms{};
    HRESULT hr = context->Map(sb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    assert(SUCCEEDED(hr));
    memcpy(ms.pData, data, size);
    context->Unmap(sb, 0);
}

void FrameResources::Create(ID3D11Device* device)
{
    // Context Buffer 생성
    CreateDynamicCB(device, sizeof(CBFrame), &m_CBFrame);
    CreateDynamicCB(device, sizeof(CBObject), &m_CBObject);
    CreateDynamicCB(device, sizeof(CBLight), &m_CBLight);
    CreateDynamicCB(device, sizeof(CBShadow), &m_CBShadow);
    CreateDynamicCB(device, sizeof(CBShadow), &m_CBLightPhong);
    CreateDynamicCB(device, sizeof(CBBoneMeta), &m_CBBoneMeta);
    
    {
        const UINT stride = sizeof(DirectX::XMFLOAT4X4);
        UINT kMaxBonesSB = 2048;
        const UINT byteWidth = stride * kMaxBonesSB;
        CreateDynamicStructuredBuffer(device, byteWidth, stride, m_BoneSB.GetAddressOf());
        CreateStructuredBufferSRV(device, m_BoneSB.Get(), kMaxBonesSB, m_BoneSRV.GetAddressOf());
    }

    // Smapler
    D3D11_SAMPLER_DESC sd{};
    ZeroMemory(&sd, sizeof(sd));
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    CheckHR(device->CreateSamplerState(&sd, m_CommonSampler.GetAddressOf()));
    
    sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sd.ComparisonFunc = D3D11_COMPARISON_LESS;
    CheckHR(device->CreateSamplerState(&sd, m_ShadowSampler.GetAddressOf()));
    
    // cube map
    {
        D3D11_SAMPLER_DESC sampDesc;
        ZeroMemory(&sampDesc, sizeof(sampDesc));
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
        CheckHR(device->CreateSamplerState(&sampDesc, m_CubeMapSampler.GetAddressOf()));
    }

}

void FrameResources::BindCommon(ID3D11DeviceContext* context)
{
    // ConstantBuffers
    ID3D11Buffer* vsCBs[] = { m_CBFrame.Get(), m_CBObject.Get(), m_CBLight.Get(), m_CBShadow.Get(), m_CBLightPhong.Get(), m_CBBoneMeta.Get() };
    ID3D11Buffer* psCBs[] = { m_CBFrame.Get(), m_CBObject.Get(), m_CBLight.Get(), m_CBShadow.Get(), m_CBLightPhong.Get() };
    context->VSSetConstantBuffers(0, 6, vsCBs);
    context->PSSetConstantBuffers(0, 5, psCBs);
    context->GSSetConstantBuffers(0, 5, psCBs);
    
    ID3D11ShaderResourceView* boneSrv = m_BoneSRV.Get();
    context->VSSetShaderResources(0, 1, &boneSrv);

    // Sampler
    ID3D11SamplerState* samplers[] = { m_CommonSampler.Get(), m_ShadowSampler.Get(), m_CubeMapSampler.Get()  };
    context->PSSetSamplers(0, 3, samplers);
}

void FrameResources::UpdateFrame(ID3D11DeviceContext* ctx, const CBFrame& data)
{
    UpdateCB(ctx, m_CBFrame.Get(),  &data, sizeof(data));
}

void FrameResources::UpdateObject(ID3D11DeviceContext* ctx, const CBObject& data)
{
    UpdateCB(ctx, m_CBObject.Get(),  &data, sizeof(data));
}

void FrameResources::UpdateLight(ID3D11DeviceContext* ctx, const CBLight& data)
{
    UpdateCB(ctx, m_CBLight.Get(),  &data, sizeof(data));
}

void FrameResources::UpdateShadow(ID3D11DeviceContext* ctx, const CBShadow& data)
{
    UpdateCB(ctx, m_CBShadow.Get(),  &data, sizeof(data));
}

void FrameResources::UpdatePhong(ID3D11DeviceContext* ctx, const CBLightPhong& data)
{
    UpdateCB(ctx, m_CBLightPhong.Get(),  &data, sizeof(data));
}

void FrameResources::UpdateBones(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4X4* bones, uint32_t boneCount)
{
    if (!ctx || !bones || boneCount == 0) return;

    // 1) StructuredBuffer 업데이트
    UpdateSB(ctx, m_BoneSB.Get(), bones, sizeof(DirectX::XMFLOAT4X4) * boneCount);

    // 2) Meta CB 업데이트 (b6)
    CBBoneMeta meta{};
    meta.BoneCount = boneCount;
    UpdateCB(ctx, m_CBBoneMeta.Get(), &meta, sizeof(meta));
}

