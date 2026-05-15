#include "Win32Window.h"

#include <cassert>
#include <imgui.h>

static const wchar_t* kWndClassName = L"D3D11EngineWnd";


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

Win32Window::Win32Window(HINSTANCE hInst, int width, int height, const wchar_t* title)
    : m_Width(width), m_Height(height), m_Title(title)
{
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32Window::WndProcSetup;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWndClassName;
    RegisterClassEx(&wc);

    RECT rc{ 0, 0, width, height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_Hwnd = CreateWindowEx(
        0, kWndClassName, title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, this);

    assert(m_Hwnd);
    ShowWindow(m_Hwnd, SW_SHOW);
}

Win32Window::~Win32Window()
{
    if (m_Hwnd)
    {
        DestroyWindow(m_Hwnd);
    }
}

bool Win32Window::PumpMessages()
{
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

void Win32Window::Tick(float DeltaTime)
{
    m_Input->BeginFrame();
}

LRESULT Win32Window::WndProcSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        Win32Window* wnd = reinterpret_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(wnd));
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Win32Window::WndProcThunk));
        return wnd->WndProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT Win32Window::WndProcThunk(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* wnd = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    return wnd->WndProc(hWnd, message, wParam, lParam);
}

LRESULT Win32Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // UI 입력이 먼저
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }
        
    // 입력 먼저 먹이고
    if (m_Input != nullptr)
    {
        m_Input->OnMsg(message, wParam, lParam);
    }

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: // Resize는 일단 여기서 구현
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);

        if (wParam != SIZE_MINIMIZED && m_OnResize != nullptr)
        {
            m_OnResize(width, height);
        }
        return 0;

    }
    return DefWindowProc(hWnd, message, wParam, lParam);

    /*
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);

        if (wParam != SIZE_MINIMIZED && m_OnResize != nullptr)
        {
            m_OnResize(width, height);
        }
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
    */
}
