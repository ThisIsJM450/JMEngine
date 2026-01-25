#include "Timer.h"

Timer::Timer()
{
    LARGE_INTEGER f{};
    ::QueryPerformanceFrequency(&f);
    m_Freq = f.QuadPart;

    ::QueryPerformanceCounter(&m_Prev);
}

float Timer::Tick()
{
    LARGE_INTEGER now{};
    ::QueryPerformanceCounter(&now);

    const LONGLONG delta = now.QuadPart - m_Prev.QuadPart;
    m_Prev = now;

    double dt = (double)delta / (double)m_Freq;

    // (옵션) 디버그/일시정지 후 큰 dt 튐 방지
    if (dt > 0.1) dt = 0.1;

    return (float)dt;
}
