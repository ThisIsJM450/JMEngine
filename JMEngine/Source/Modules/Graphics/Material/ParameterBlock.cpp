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
    if (IsEXR(filename))
    {
        DirectX::ScratchImage image;
        DirectX::TexMetadata metadata;

        std::wstring wfilename(filename.begin(), filename.end());

        HRESULT hr = DirectX::LoadFromEXRFile(
            wfilename.c_str(),
            &metadata,
            image);

        if (FAILED(hr))
        {
            assert(false && "Failed to load EXR");
            return;
        }

        // DirectXTex는 Resource로 만들어줌
        Microsoft::WRL::ComPtr<ID3D11Resource> res;

        // 텍스처 생성 (metadata.format은 보통 R16G16B16A16_FLOAT)
        hr = DirectX::CreateTexture(
            dev,
            image.GetImages(),
            image.GetImageCount(),
            metadata,
            res.GetAddressOf());

        if (FAILED(hr))
        {
            assert(false && "Failed to create EXR texture");
            return;
        }

        // SRV 생성
        hr = DirectX::CreateShaderResourceView(
            dev,
            image.GetImages(),
            image.GetImageCount(),
            metadata,
            textureResourceView.GetAddressOf());

        if (FAILED(hr))
        {
            assert(false && "Failed to create EXR SRV");
            return;
        }

        // ID3D11Texture2D로 캐스팅해서 기존 인터페이스 유지
        hr = res.As(&texture);
        if (FAILED(hr))
        {
            assert(false && "EXR texture is not ID3D11Texture2D");
            return;
        }

        return;
    }
    
    // ===============================
    // DDS 경로 (GPU 압축 텍스처)
    // ===============================
    // if (IsDDS(filename))
    // {
    //     DirectX::ScratchImage image;
    //     DirectX::TexMetadata metadata;
    //
    //     std::wstring wfilename(filename.begin(), filename.end());
    //
    //     HRESULT hr = DirectX::LoadFromDDSFile(
    //         wfilename.c_str(),
    //         DirectX::DDS_FLAGS_NONE,
    //         &metadata,
    //         image);
    //
    //     if (FAILED(hr))
    //     {
    //         assert(false && "Failed to load DDS");
    //         return;
    //     }
    //
    //     hr = DirectX::CreateTexture(
    //         dev,
    //         image.GetImages(),
    //         image.GetImageCount(),
    //         metadata,
    //         reinterpret_cast<ID3D11Resource**>(texture.GetAddressOf()));
    //
    //     if (FAILED(hr))
    //     {
    //         assert(false && "Failed to create DDS texture");
    //         return;
    //     }
    //
    //     hr = DirectX::CreateShaderResourceView(
    //         dev,
    //         image.GetImages(),
    //         image.GetImageCount(),
    //         metadata,
    //         textureResourceView.GetAddressOf());
    //
    //     if (FAILED(hr))
    //     {
    //         assert(false && "Failed to create DDS SRV");
    //     }
    //
    //     return;
    // }

    // ===============================
    // 일반 이미지 (stb_image)
    // ===============================
    int width, height, channels;

    unsigned char* img =
        stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!img)
    {
        assert(false && "Failed to load image");
        return;
    }

    // Create texture.
    D3D11_TEXTURE2D_DESC txtDesc = {};
    txtDesc.Width = width;
    txtDesc.Height = height;
    txtDesc.MipLevels = 1;
    txtDesc.ArraySize = 1;
    txtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    txtDesc.SampleDesc.Count = 1;
    txtDesc.Usage = D3D11_USAGE_IMMUTABLE;
    txtDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = img;
    initData.SysMemPitch = width * 4;

    HRESULT hr = dev->CreateTexture2D(
        &txtDesc,
        &initData,
        texture.GetAddressOf());

    stbi_image_free(img);

    if (FAILED(hr))
    {
        assert(false && "CreateTexture2D failed");
        return;
    }

    
    hr = dev->CreateShaderResourceView(
        texture.Get(),
        nullptr,
        textureResourceView.GetAddressOf());

    if (FAILED(hr))
    {
        assert(false && "CreateShaderResourceView failed");
    }
}

bool ParameterBlock::IsEXR(const std::string& filename)
{
    std::filesystem::path p(filename);
    auto ext = p.extension().string();
    for (auto& c : ext) c = (char)tolower(c);
    return ext == ".exr";
}

// bool ParameterBlock::IsDDS(const std::string& filename)
// {
//     std::filesystem::path p(filename);
//     return p.extension() == ".dds" || p.extension() == ".DDS";
// }
