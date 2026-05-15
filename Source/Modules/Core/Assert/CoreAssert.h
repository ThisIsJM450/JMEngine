#pragma once
#include <iostream>
#include <Windows.h>
#include <winbase.h>

#include <winuser.h>


class CoreAssert
{
public:
    static void CheckHR(HRESULT hr, const wchar_t* msg)
    {
        if (SUCCEEDED(hr)) return;

        if ((hr & D3D11_ERROR_FILE_NOT_FOUND) != 0) {
            std::cout << "File not found." << std::endl;
        }

        wchar_t sysMsg[1024] = {};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            sysMsg, (unsigned long)_countof(sysMsg), nullptr);

        wchar_t finalMsg[2048] = {};
        _snwprintf_s(finalMsg, _countof(finalMsg), _TRUNCATE,
            L"%s\nHRESULT=0x%08X\n%s", msg ? msg : L"(no msg)", (unsigned)hr, sysMsg);

        OutputDebugStringW(finalMsg);
        OutputDebugStringW(L"\n");
        MessageBoxW(nullptr, finalMsg, L"CheckHR Failed", MB_ICONERROR | MB_OK);
        abort();
    }
};
