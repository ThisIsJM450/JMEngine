#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "ShaderProgram.h"
#include "../Scene/Scene.h"
#include "Material/MaterialInstance.h"


//struct MeshData;
class Material;


struct alignas(16) CBPerDraw
{
    DirectX::XMMATRIX WorldViewProj; // transposed
    DirectX::XMFLOAT4 Color;
};

class Dx11Context
{
public:
    Dx11Context(HWND hwnd, int width, int height);
    static const Dx11Context& Get() { return *M_Instance; }

    void InitBasicPipeline(const wchar_t* forwardHlslPath, const wchar_t* shadowHlslPath, const wchar_t* gsHlslPath = nullptr);

    void BeginFrame();
    // void RenderScene(const Scene& scene);
    void EndFrame();

    // MeshFactory에서 사용할 유틸
    void CreateImmutableBuffer(const void* data, UINT byteSize, UINT bindFlags,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& out);

    ID3D11Device* GetDevice() const { return m_Device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_Ctx.Get(); }

    void Resize(int newWidth, int newHeight);

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    void BindBackbuffer();

    // void DrawMeshForward(const GPUMesh& meshData, Material* material, const CBPerDraw& cb);

    Material* GetBasicMaterial() const {return m_BasicMaterial.get();}
    const std::shared_ptr<Material> GetSharedBasicMaterial() const { return m_BasicMaterial; }
    MaterialInstance* GetBasicMaterialInstance() const;  
    
    //~ Start HDR
    void CreateHDRRTVAndDSV();
    // void BindHDRTarget();
    // void ToneMapToBackbuffer();
    // ~ End HDR
    
    // Backbuffer
    ID3D11RenderTargetView* GetBackbufferRTV() const { return m_RTV.Get(); }
    ID3D11DepthStencilView* GetBackbufferDSV() const { return m_DSV.Get(); }

    // HDR Scene
    ID3D11RenderTargetView* GetHDRRTV() const { return m_HDRRTV.Get(); }
    // ID3D11DepthStencilView* GetHDRDSV() const { return m_HDRDSV.Get(); }
    ID3D11ShaderResourceView* GetHDRSRV() const { return m_HDRSRV.Get(); }
    
    static Dx11Context* M_Instance;

private:
    void CreateDeviceAndSwapChain(HWND hwnd);
    void CreateRTVAndDSV();
    void SetViewport();

private:
    int m_Width = 0;
    int m_Height = 0;

    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Ctx;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DSV;
    // Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_RS;

    // ShaderProgram m_BasicShader;
    // Microsoft::WRL::ComPtr<ID3D11Buffer> m_CBPerDraw;

    std::shared_ptr<Material> m_BasicMaterial;
    std::shared_ptr<MaterialInstance> m_BasicMaterialInstance;
    
    // HDR Scene
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_HDRTex;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_HDRRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_HDRSRV;

    // HDR Depth
    // Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_HDRDepthTex;
    // Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_HDRDSV;

};
