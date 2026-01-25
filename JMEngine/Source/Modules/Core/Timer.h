#pragma once

#include <Windows.h>



class Timer
{
public:
    Timer();

    // 초 단위 dt 반환
    float Tick();

private:
    LONGLONG m_Freq = 0;
    LARGE_INTEGER m_Prev{};
};
