#pragma once
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace ShaderCompileUtils
{
    Microsoft::WRL::ComPtr<ID3DBlob> Compile(const wchar_t* path, const char* entry, const char* target);
}