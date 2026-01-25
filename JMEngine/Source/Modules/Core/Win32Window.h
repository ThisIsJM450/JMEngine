#pragma once
#include <functional>
#include <memory>
#include <string>
#include <Windows.h>

#include "../Input/Input.h"

class Win32Window
{
public:
    Win32Window(HINSTANCE hInstance, int width, int height, const wchar_t* title);
    ~Win32Window();

    HWND GetHwnd() const { return m_Hwnd; }
    bool PumpMessages();
    void SetOnResize(std::function<void(int, int)> cb) { m_OnResize = std::move(cb); }
    void Tick(float DeltaTime);

    void SetInput(std::shared_ptr<Input> input) { m_Input = input; }

private:
    static LRESULT CALLBACK WndProcSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProcThunk(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND m_Hwnd = nullptr;
    
    int m_Width = 0;
    int m_Height = 0;
    std::wstring m_Title;
    std::function<void(int, int)> m_OnResize;
    std::shared_ptr<Input> m_Input;
};
