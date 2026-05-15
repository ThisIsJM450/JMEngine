#include "Input.h"

#include <cstdint>
#include <cstring>
#include <winuser.h>
#include <Windows.h>
#include <Windowsx.h>

void InputSystem::BeginFrame()
{
    std::memset(keyPressed,  0, sizeof(keyPressed));
    std::memset(keyReleased, 0, sizeof(keyReleased));

    lmbPressed = rmbPressed = mmbPressed = false;
    lmbReleased = rmbReleased = mmbReleased = false;

    mouseDeltaX = 0;
    mouseDeltaY = 0;
    wheelDelta  = 0;

    prevMouseX = mouseX;
    prevMouseY = mouseY;
}

void InputSystem::OnMsg(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SETFOCUS:
        hasFocus = true;
        break;

    case WM_KILLFOCUS:
        hasFocus = false;
        // 포커스 잃을 때 stuck 방지
        std::memset(keyDown, 0, sizeof(keyDown));
        lmbDown = rmbDown = mmbDown = false;
        break;

    // Keyboard
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        const uint32_t vk = (uint32_t)wParam & 0xFF;
        const bool wasDown = (lParam & (1 << 30)) != 0; // 이전 상태
        keyDown[vk] = true;
        if (!wasDown) keyPressed[vk] = true; // auto-repeat 방지
    } break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        const uint32_t vk = (uint32_t)wParam & 0xFF;
        keyDown[vk] = false;
        keyReleased[vk] = true;
    } break;

    // Mouse move
    case WM_MOUSEMOVE:
        mouseX = static_cast<float>(GET_X_LPARAM(lParam));
        mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

        // 메시지 기반 delta 누적(프레임당 여러번 올 수 있음)
        mouseDeltaX += (mouseX - prevMouseX);
        mouseDeltaY += (mouseY - prevMouseY);

        // prev를 갱신해줘야 누적이 자연스러움
        prevMouseX = mouseX;
        prevMouseY = mouseY;
        break;

    // Mouse buttons
    case WM_LBUTTONDOWN:
        lmbDown = true; lmbPressed = true;
        break;
    case WM_LBUTTONUP:
        lmbDown = false; lmbReleased = true;
        break;

    case WM_RBUTTONDOWN:
        rmbDown = true; rmbPressed = true;
        break;
    case WM_RBUTTONUP:
        rmbDown = false; rmbReleased = true;
        break;

    case WM_MBUTTONDOWN:
        mmbDown = true; mmbPressed = true;
        break;
    case WM_MBUTTONUP:
        mmbDown = false; mmbReleased = true;
        break;

    // Wheel
    case WM_MOUSEWHEEL:
        wheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
        break;

    default:
        break;
    }
}
