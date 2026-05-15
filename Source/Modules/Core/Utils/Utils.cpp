#include "Utils.h"
//
// #include <winnls.h>
//
// wchar_t* Utils::ToWString(const std::string& s)
// {
//     if (s.empty())
//     {
//         wchar_t* out = new wchar_t[1]{ L'\0' };
//         return out;
//     }
//
//     const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
//     wchar_t* out = new wchar_t[len];
//     MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out, len);
//     return out;
// }
