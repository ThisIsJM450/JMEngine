#pragma once
//#include <winuser.h>

#include "../Scene/Scene.h"

class Input
{
public:

    // 입력 처리
    void Tick(float DeltaTime, Scene& scene);
    
    // 프레임 시작 시 호출: 프레임성 값 초기화
    void BeginFrame();

    // 메시지 처리 (WndProc에서 호출)
    void OnMsg(UINT msg, WPARAM wParam, LPARAM lParam);

    // 편의 함수들
    bool KeyDown(int vk) const      { return keyDown[vk & 0xFF]; }
    bool KeyPressed(int vk) const   { return keyPressed[vk & 0xFF]; }
    bool KeyReleased(int vk) const  { return keyReleased[vk & 0xFF]; }

    bool ShiftDown() const { return KeyDown(VK_SHIFT); }
    bool CtrlDown()  const { return KeyDown(VK_CONTROL); }
    bool AltDown()   const { return KeyDown(VK_MENU); }

    bool RMBDown()   const { return rmbDown; }

    float MouseDeltaX() const { return mouseDeltaX; }
    float MouseDeltaY() const { return mouseDeltaY; }
    float MouseWheel()  const { return wheelDelta; }


private:
    // Keyboard
    bool keyDown[256]{};
    bool keyPressed[256]{};   // 이번 프레임에 눌림 (Down edge)
    bool keyReleased[256]{};  // 이번 프레임에 뗌 (Up edge)

    // Mouse buttons
    bool lmbDown = false;
    bool rmbDown = false;
    bool mmbDown = false;

    bool lmbPressed = false;
    bool rmbPressed = false;
    bool mmbPressed = false;

    bool lmbReleased = false;
    bool rmbReleased = false;
    bool mmbReleased = false;

    // Mouse position (client coords)
    float mouseX = 0;
    float mouseY = 0;
    float prevMouseX = 0;
    float prevMouseY = 0;

    // Per-frame deltas
    float mouseDeltaX = 0;
    float mouseDeltaY = 0;
    float wheelDelta  = 0; // WM_MOUSEWHEEL 누적 (WHEEL_DELTA 단위)

    // Focus
    bool hasFocus = true;
};
