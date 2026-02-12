#include "ParameterBlock.h"

#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>
#include <DirectXTex.h>
#include <DirectXTexEXR.h>

#include "stb_image.h"
#include <vector>
#include "../../Core/Assert/CoreAssert.h"


// void ParameterBlock::Init(ID3D11Device* dev)
// {
//     // m_Device = dev;
//     EnsureCB(dev);
// }

void ParameterBlock::SetTexture(uint32_t slot, const std::string& TextureFileName)
{
    assert(slot < kMaxTextures);
    m_dirtyTextureFileName[slot] = TextureFileName;
}

void ParameterBlock::SetTexture(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    assert(slot < kMaxTextures);
    m_SRV[slot] = srv;
}

void ParameterBlock::SetSampler(uint32_t slot, ID3D11SamplerState* samp)
{
    assert(slot < kMaxSamplers);
    m_Samplers[slot] = samp;
}

void ParameterBlock::Bind(ID3D11Device* dev, ID3D11DeviceContext* ctx, uint32_t materialCBRegister, uint32_t texStartRegister,
    uint32_t sampStartRegister)
{
    EnsureCB(dev);
    UpdateCBIfDirty(ctx);

    ID3D11Buffer* cb = m_CBuffer.Get();
    ctx->VSSetConstantBuffers(materialCBRegister, 1, &cb);
    ctx->PSSetConstantBuffers(materialCBRegister, 1, &cb);
    ctx->GSSetConstantBuffers(materialCBRegister, 1, &cb);
    
    ID3D11ShaderResourceView* srvs[kMaxTextures]{};
    for (uint32_t i = 0; i < kMaxTextures; ++i)
    {
        if (m_dirtyTextureFileName[i].empty() == false)
        {
            CreateTexture(dev, m_dirtyTextureFileName[i], m_Textures[i], m_SRV[i]);
            m_dirtyTextureFileName[i].clear();
        }
    }
    for (uint32_t i = 0; i < kMaxTextures; ++i)
    {
        srvs[i] = m_SRV[i].Get();
    }
    ctx->PSSetShaderResources(texStartRegister, kMaxTextures, srvs);

    ID3D11SamplerState* samps[kMaxSamplers]{};
    for (uint32_t i = 0; i < kMaxSamplers; ++i)
    {
        samps[i] = m_Samplers[i].Get();
    }
    ctx->PSSetSamplers(sampStartRegister, kMaxSamplers, samps);
}

void ParameterBlock::EnsureCB(ID3D11Device* dev)
{
    if (m_CBuffer) return;

    D3D11_BUFFER_DESC desc{};
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = (UINT)sizeof(CBMaterial);

    CoreAssert::CheckHR(dev->CreateBuffer(&desc, nullptr, m_CBuffer.GetAddressOf()), L"Create CBMaterial failed");
    m_Dirty = true;
}

void ParameterBlock::UpdateCBIfDirty(ID3D11DeviceContext* ctx)
{
    if (!m_Dirty) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    CoreAssert::CheckHR(ctx->Map(m_CBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), L"Map CBMaterial failed");
    memcpy(mapped.pData, &m_CB, sizeof(CBMaterial));
    ctx->Unmap(m_CBuffer.Get(), 0);

    m_Dirty = false;
}

void ParameterBlock::CreateTexture(ID3D11Device* dev, const std::string filename, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& textureResourceView)
{
auto ToWString = [](const std::string& s) -> std::wstring
    {
        // 간단 변환(ASCII만 안전). 실제 서비스면 UTF-8 -> UTF-16 변환 권장.
        return std::wstring(s.begin(), s.end());
    };

    const std::wstring wfilename = ToWString(filename);

    // -----------------------
    // EXR (HDR, Linear)
    // -----------------------
    if (IsEXR(filename))
    {
        DirectX::ScratchImage image;
        DirectX::TexMetadata metadata;

        HRESULT hr = DirectX::LoadFromEXRFile(wfilename.c_str(), &metadata, image);
        if (FAILED(hr)) { assert(false && "Failed to load EXR"); return; }

        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        hr = DirectX::CreateTexture(dev, image.GetImages(), image.GetImageCount(), metadata, res.GetAddressOf());
        if (FAILED(hr)) { assert(false && "Failed to create EXR texture"); return; }

        hr = DirectX::CreateShaderResourceView(dev, image.GetImages(), image.GetImageCount(), metadata, textureResourceView.GetAddressOf());
        if (FAILED(hr)) { assert(false && "Failed to create EXR SRV"); return; }

        hr = res.As(&texture);
        if (FAILED(hr)) { assert(false && "EXR texture is not ID3D11Texture2D"); return; }

        return;
    }

    // -----------------------
    // DDS (2D / Cubemap / BC6H etc.)
    // -----------------------
    if (IsDDS(filename))
    {
        DirectX::ScratchImage image;
        DirectX::TexMetadata metadata;

        HRESULT hr = DirectX::LoadFromDDSFile(
            wfilename.c_str(),
            DirectX::DDS_FLAGS_NONE,   // 필요 시 DDS_FLAGS_NO_16BPP, DDS_FLAGS_FORCE_RGB 등
            &metadata,
            image);

        if (FAILED(hr)) { assert(false && "Failed to load DDS"); return; }

        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        hr = DirectX::CreateTexture(dev, image.GetImages(), image.GetImageCount(), metadata, res.GetAddressOf());
        if (FAILED(hr)) { assert(false && "Failed to create DDS texture"); return; }

        // metadata 기반으로 SRV Dimension(TEXTURE2D / TEXTURECUBE 등) 알아서 맞춰줌
        hr = DirectX::CreateShaderResourceView(dev, image.GetImages(), image.GetImageCount(), metadata, textureResourceView.GetAddressOf());
        if (FAILED(hr)) { assert(false && "Failed to create DDS SRV"); return; }

        // 큐브맵도 ID3D11Texture2D (ArraySize=6)로 캐스팅 가능
        hr = res.As(&texture);
        if (FAILED(hr)) { assert(false && "DDS texture is not ID3D11Texture2D"); return; }

        return;
    }

    // -----------------------
    // 일반 이미지 (stb_image) - 주의: 이건 무조건 SRGB로 만들고 있음
    // -----------------------
    int width, height, channels;
    unsigned char* img = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!img) { assert(false && "Failed to load image"); return; }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // <- 알베도용이면 OK, 데이터/IBL이면 보통 NON_SRGB
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = img;
    init.SysMemPitch = width * 4;

    HRESULT hr = dev->CreateTexture2D(&desc, &init, texture.GetAddressOf());
    stbi_image_free(img);
    if (FAILED(hr)) { assert(false && "CreateTexture2D failed"); return; }

    hr = dev->CreateShaderResourceView(texture.Get(), nullptr, textureResourceView.GetAddressOf());
    if (FAILED(hr)) { assert(false && "CreateShaderResourceView failed"); return; }
}

bool ParameterBlock::IsEXR(const std::string& filename)
{
    std::filesystem::path p(filename);
    auto ext = p.extension().string();
    for (auto& c : ext) c = (char)tolower(c);
    return ext == ".exr";
}

bool ParameterBlock::IsDDS(const std::string& filename)
{
    std::filesystem::path p(filename);
    return p.extension() == ".dds" || p.extension() == ".DDS";
}
