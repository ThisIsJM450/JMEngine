#pragma once
//#include <winuser.h>

#include <Windows.h>

class InputSystem
{
public:
    void BeginFrame();
    void OnMsg(UINT msg, WPARAM wParam, LPARAM lParam);

    bool KeyDown(int vk) const      { return keyDown[vk & 0xFF]; }
    bool KeyPressed(int vk) const   { return keyPressed[vk & 0xFF]; }
    bool KeyReleased(int vk) const  { return keyReleased[vk & 0xFF]; }

    bool ShiftDown() const { return KeyDown(VK_SHIFT); }
    bool CtrlDown()  const { return KeyDown(VK_CONTROL); }
    bool AltDown()   const { return KeyDown(VK_MENU); }

    bool LMBDown() const { return lmbDown; }
    bool RMBDown() const { return rmbDown; }
    bool MMBDown() const { return mmbDown; }

    bool LMBPressed() const { return lmbPressed; }
    bool RMBPressed() const { return rmbPressed; }
    bool MMBPressed() const { return mmbPressed; }

    bool LMBReleased() const { return lmbReleased; }
    bool RMBReleased() const { return rmbReleased; }
    bool MMBReleased() const { return mmbReleased; }

    float MouseX() const { return mouseX; }
    float MouseY() const { return mouseY; }
    float MouseDeltaX() const { return mouseDeltaX; }
    float MouseDeltaY() const { return mouseDeltaY; }
    float MouseWheel()  const { return wheelDelta; }
    bool HasFocus() const { return hasFocus; }

private:
    bool keyDown[256]{};
    bool keyPressed[256]{};
    bool keyReleased[256]{};

    bool lmbDown = false;
    bool rmbDown = false;
    bool mmbDown = false;

    bool lmbPressed = false;
    bool rmbPressed = false;
    bool mmbPressed = false;

    bool lmbReleased = false;
    bool rmbReleased = false;
    bool mmbReleased = false;

    float mouseX = 0;
    float mouseY = 0;
    float prevMouseX = 0;
    float prevMouseY = 0;

    float mouseDeltaX = 0;
    float mouseDeltaY = 0;
    float wheelDelta  = 0;

    bool hasFocus = true;
};

using Input = InputSystem;
