#include "Modules/Core/AppBase.h"
#include "Modules/Test/RootMotionHeadlessCli.h"

int main(int argc, char** argv)
{
    int cliExitCode = 0;
    if (Test::TryRunRootMotionHeadlessCli(argc, argv, cliExitCode))
    {
        return cliExitCode;
    }

    AppDesc desc{};
    desc.hInstance = GetModuleHandleW(nullptr);
    desc.Width = 1280;
    desc.Height = 720;
    desc.WindowTitle = L"D3D11 Mini Engine (Console main)";

    AppBase app(desc);
    return app.Run();
}
