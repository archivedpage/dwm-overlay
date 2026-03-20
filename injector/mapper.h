#pragma once

#include <Windows.h>

namespace mapper {
    bool inject(HANDLE process, const wchar_t* dll_path);
}
