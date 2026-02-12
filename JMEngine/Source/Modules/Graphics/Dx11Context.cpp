#include "Dx11Context.h"
#include "../Scene/Scene.h"

#include "../Renderer/RenderStruct.h"

#include <iostream>

#include "../Renderer/FrameResources.h"
#include "Material/Material.h"
#include "Texture/Texture.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

Dx11Context* Dx11Context::M_Instance = nullptr;

static void CheckHR(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        std::cout << msg << std::endl;
        assert(false); abort();

    }
}

Dx11Context::Dx11Context(HWND hwnd, int width, int height)
    : m_Width(width), m_Height(height)
{
    CreateDeviceAndSwapChain(hwnd);
    CreateRTVAndDSV();
    CreateHDRRTVAndDSV();
    
    assert(M_Instance == nullptr); // 싱글톤 기대
    M_Instance = this;
}

void Dx11Context::InitBasicPipeline(const wchar_t* forwardHlslPath, const wchar_t* shadowHlslPath, const wchar_t* gsHlslPath)
{
    m_BasicMaterial = std::make_shared<Material>();
    m_BasicMaterial->psPath = forwardHlslPath;
    m_BasicMaterial->psPath_Shadow = shadowHlslPath;
    m_BasicMaterial->gsPath = gsHlslPath;

    m_BasicMaterialInstance = std::make_shared<MaterialInstance>(m_BasicMaterial);
    m_BasicMaterialInstance->SetTexture(0, TextureFileName::GetTexture_Wall());
    
    m_StoneBrickMaterialInstance = std::make_shared<MaterialInstance>(m_BasicMaterial);
    {
        // 슬롯 가정: 0=Albedo(BaseColor), 1=Normal, 2=MetallicRoughness(또는 Metallic), 3=AO, 4=Emissive

        m_StoneBrickMaterialInstance->SetTexture(0, TextureFileName::GetTexture_StoneBricks_Albedo());
        m_StoneBrickMaterialInstance->SetTexture(1, TextureFileName::GetTexture_StoneBricks_Normal());
        m_StoneBrickMaterialInstance->SetTexture(2, TextureFileName::GetTexture_StoneBricks_Gloss());
        m_StoneBrickMaterialInstance->SetTexture(3, TextureFileName::GetTexture_StoneBricks_AO());
        m_StoneBrickMaterialInstance->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
        m_StoneBrickMaterialInstance->SetRoughness(0.5f);
        m_StoneBrickMaterialInstance->SetMetallic(0.f);
        m_StoneBrickMaterialInstance->SetUseGlossMap(true);
        m_StoneBrickMaterialInstance->SetUseNormalMap(true);
        m_StoneBrickMaterialInstance->SetUsePackedMetalRough(true);
    }

    
    m_MetalGoldPaintMaterialInstance = std::make_shared<MaterialInstance>(m_BasicMaterial);
    {
        m_MetalGoldPaintMaterialInstance->SetTexture(0, TextureFileName::GetTexture_MetalGoldPaint_Albedo());
        m_MetalGoldPaintMaterialInstance->SetTexture(1, TextureFileName::GetTexture_MetalGoldPaint_Normal());
        m_MetalGoldPaintMaterialInstance->SetTexture(2, TextureFileName::GetTexture_MetalGoldPaint_Metallic());
        m_MetalGoldPaintMaterialInstance->SetTexture(3, TextureFileName::GetTexture_MetalGoldPaint_AO());
        m_MetalGoldPaintMaterialInstance->SetTexture(5, TextureFileName::GetTexture_MetalGoldPaint_Roughness());
        m_MetalGoldPaintMaterialInstance->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
        m_MetalGoldPaintMaterialInstance->SetRoughness(0.3f);
        m_MetalGoldPaintMaterialInstance->SetMetallic(1.f);
        m_MetalGoldPaintMaterialInstance->SetUseNormalMap(true);
    }

    
    m_GrassPatchyGroundMaterialInstance = std::make_shared<MaterialInstance>(m_BasicMaterial);
    {
        m_GrassPatchyGroundMaterialInstance->SetTexture(0, TextureFileName::GetTexture_GrassPatchyGround_Albedo());
        m_GrassPatchyGroundMaterialInstance->SetTexture(1, TextureFileName::GetTexture_GrassPatchyGround_Normal());
        m_GrassPatchyGroundMaterialInstance->SetTexture(2, TextureFileName::GetTexture_GrassPatchyGround_Metallic());
        m_GrassPatchyGroundMaterialInstance->SetTexture(3, TextureFileName::GetTexture_GrassPatchyGround_AO());
        m_GrassPatchyGroundMaterialInstance->SetTexture(5, TextureFileName::GetTexture_GrassPatchyGround_Roughness());
        m_GrassPatchyGroundMaterialInstance->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
        m_GrassPatchyGroundMaterialInstance->SetRoughness(1.f);
        m_GrassPatchyGroundMaterialInstance->SetMetallic(0.0f);
        m_GrassPatchyGroundMaterialInstance->SetUseNormalMap(true);
    }
    
    m_WhiteMaterialInstance = std::make_shared<MaterialInstance>(m_BasicMaterial);
    {
        m_WhiteMaterialInstance->SetTexture(0, TextureFileName::GetTexture_White());
    }

}

void Dx11Context::BeginFrame()
{
    constexpr float clear[4] = { 0.07f, 0.07f, 0.10f, 1.0f };
    
    if (m_HDRRTV )
    {
        m_Ctx->ClearRenderTargetView(m_HDRRTV.Get(), clear);
    }
    
    if (m_RTV && m_DSV)
    {
        m_Ctx->ClearRenderTargetView(m_RTV.Get(), clear);
        m_Ctx->ClearDepthStencilView(m_DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
   

    SetViewport();
}

void Dx11Context::EndFrame()
{
    m_SwapChain->Present(1, 0);
}

void Dx11Context::CreateImmutableBuffer(const void* data, UINT byteSize, UINT bindFlags, ComPtr<ID3D11Buffer>& out)
{
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = byteSize;
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = bindFlags;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = data;

    CheckHR(
        m_Device->CreateBuffer(&bd, &init, &out),
        L"CreateImmutableBuffer is failed. CreateBuffer"
        );
}

void Dx11Context::Resize(int newWidth, int newHeight)
{
    // 1 Pixel은 남겨두자..
    if (newWidth <= 0 || newHeight <= 0)
    {
        return;
    }

    // 변경사항이 없음
    if (newWidth == m_Width && newHeight == m_Height)
    {
        return;
    }

    m_Width = newWidth;
    m_Height = newHeight;

    // Unbind RT
    ID3D11RenderTargetView* rtv[1] = {nullptr};
    m_Ctx->OMSetRenderTargets(1, rtv, nullptr);

    m_RTV.Reset();
    m_DSV.Reset();
    m_SRV.Reset();
    m_BackBufferRTV.Reset();

    // 스왑 체인 버퍼 리사이즈
    HRESULT hr  = m_SwapChain->ResizeBuffers(
        0, (UINT)m_Width, (UINT)m_Height, DXGI_FORMAT_UNKNOWN, 0
    );

    CheckHR(hr, L"ResizeBuffers failed");

    // RTV, DSV 재생성
    CreateRTVAndDSV();
    CreateHDRRTVAndDSV();

    // Viewport 갱신
    SetViewport();
}

void Dx11Context::BindBackbuffer()
{
    m_Ctx->OMSetRenderTargets(1, m_BackBufferRTV.GetAddressOf(), m_DSV.Get());
}

void Dx11Context::BindViewbuffer()
{
    m_Ctx->OMSetRenderTargets(1, m_RTV.GetAddressOf(), m_DSV.Get());
}

/*
void Dx11Context::DrawMeshForward(const GPUMesh& meshData, Material* material, const CBPerDraw& cb)
{
    material->Bind(m_Ctx.Get(), PassType::Forward);

    ID3D11Buffer* vb = meshData.VB.Get();
    m_Ctx->IASetVertexBuffers(0, 1, &vb, &meshData.Stride, &meshData.Offset);
    m_Ctx->IASetIndexBuffer(meshData.IB.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    CheckHR(m_Ctx->Map(m_CBPerDraw.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
            L"Map CBPerDraw failed");
    memcpy(mapped.pData, &cb, sizeof(cb));
    m_Ctx->Unmap(m_CBPerDraw.Get(), 0);

    m_Ctx->DrawIndexed(meshData.IndexCount, 0, 0);
}
*/


MaterialInstance* Dx11Context::GetBasicMaterialInstance() const
{
    return m_BasicMaterialInstance.get();
}

std::shared_ptr<MaterialInstance> Dx11Context::GetStoneBrickMaterialInstance() const
{
    return m_StoneBrickMaterialInstance;
}

std::shared_ptr<MaterialInstance> Dx11Context::GetMetalGoldPaintMaterialInstance() const
{
    return m_MetalGoldPaintMaterialInstance;
}

std::shared_ptr<MaterialInstance> Dx11Context::GetGrassPatchyGroundMaterialInstance() const
{
    return m_GrassPatchyGroundMaterialInstance;
}

std::shared_ptr<MaterialInstance> Dx11Context::GetWhiteMaterialInstance() const
{
    return m_WhiteMaterialInstance;
}

void Dx11Context::CreateHDRRTVAndDSV()
{
    // HDR Color (FP16)
    D3D11_TEXTURE2D_DESC hdrDesc{};
    hdrDesc.Width = m_Width;
    hdrDesc.Height = m_Height;
    hdrDesc.MipLevels = 1;
    hdrDesc.ArraySize = 1;
    hdrDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    hdrDesc.SampleDesc.Count = 1;
    hdrDesc.Usage = D3D11_USAGE_DEFAULT;
    hdrDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    CheckHR(
        m_Device->CreateTexture2D(&hdrDesc, nullptr, m_HDRTex.GetAddressOf()),
        L"CreateHDRRTVAndDSV() failed: HDR Texture2D"
    );

    CheckHR(
        m_Device->CreateRenderTargetView(m_HDRTex.Get(), nullptr, m_HDRRTV.GetAddressOf()),
        L"CreateHDRRTVAndDSV() failed: HDR RTV"
    );

    CheckHR(
        m_Device->CreateShaderResourceView(m_HDRTex.Get(), nullptr, m_HDRSRV.GetAddressOf()),
        L"CreateHDRRTVAndDSV() failed: HDR SRV"
    );
}

void Dx11Context::CreateDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = m_Width;
    scd.BufferDesc.Height = m_Height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL outLevel{};

    CheckHR(
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            flags, levels, 1, D3D11_SDK_VERSION,
            &scd, &m_SwapChain, &m_Device, &outLevel, &m_Ctx
        ),
        L"D3D11CreateDeviceAndSwapChain() failed."
    );
}

void Dx11Context::CreateRTVAndDSV()
{
    // RTV
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = m_Width;
        desc.Height = m_Height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // LDR 
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        m_Device->CreateTexture2D(&desc, nullptr, m_Tex.GetAddressOf());
        m_Device->CreateRenderTargetView(m_Tex.Get(), nullptr, m_RTV.GetAddressOf());
        m_Device->CreateShaderResourceView(m_Tex.Get(), nullptr, m_SRV.GetAddressOf());
        
        // backBuffer
        ComPtr<ID3D11Texture2D> backBuffer;
        m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_BackBufferRTV.GetAddressOf());
    }
    // ~ RTV

    // DSV
    D3D11_TEXTURE2D_DESC dsDesc{};
    dsDesc.Width = m_Width;
    dsDesc.Height = m_Height;
    dsDesc.MipLevels = 1;
    dsDesc.ArraySize = 1;
    dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsDesc.SampleDesc.Count = 1;
    dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTex;
    CheckHR(
    m_Device->CreateTexture2D(&dsDesc, nullptr, depthTex.GetAddressOf()),
    L"CreateRTVAndDSV() falied, because DSV Texture2D is not Created."
    );
    
    CheckHR(
    m_Device->CreateDepthStencilView(depthTex.Get(), nullptr, m_DSV.GetAddressOf()),
    L"CreateRTVAndDSV() falied, because DSV DepthStencilView is not Created."
    );
    // ~ DSV
    
}

void Dx11Context::SetViewport()
{
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = (float)m_Width;
    vp.Height = (float)m_Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_Ctx->RSSetViewports(1, &vp);
}
