#include "Modules/Core/AppBase.h"

int main()
{
    AppDesc desc{};
    desc.hInstance = GetModuleHandleW(nullptr); // 콘솔 앱에서도 HINSTANCE 얻기
    desc.Width = 1280;
    desc.Height = 720;
    desc.WindowTitle = L"D3D11 Mini Engine (Console main)";

    AppBase app(desc);
    return app.Run();
}
