#include "RenderSettings.h"

RenderSettings& RenderSettings::Get()
{
    static RenderSettings S; // 싱글톤
    return S;
}