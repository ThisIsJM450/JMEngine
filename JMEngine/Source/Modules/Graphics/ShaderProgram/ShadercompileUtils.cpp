#include "ShadercompileUtils.h"

Microsoft::WRL::ComPtr<ID3DBlob> ShaderCompileUtils::Compile(const wchar_t* path, const char* entry, const char* target)
{
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        path,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry,
        target,
        flags,
        0,
        &bytecode,
        &errors
    );

    if (FAILED(hr))
    {
        if (errors)
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
            OutputDebugStringA("\n");
        }
        OutputDebugStringW(L"[Shader compile failed] ");
        OutputDebugStringW(path);
        OutputDebugStringW(L"\n");
        return nullptr;
    }

    return bytecode;
}
