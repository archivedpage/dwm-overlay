#pragma once

#include <Windows.h>
#include <cstdint>

namespace vthook {

inline void* swap(void* instance, void* detour, int index) {
    auto vtable = *reinterpret_cast<uintptr_t**>(instance);
    auto entry = &vtable[index];

    MEMORY_BASIC_INFORMATION mbi{};
    VirtualQuery(entry, &mbi, sizeof(mbi));

    DWORD old_prot{};
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &old_prot);

    auto original = reinterpret_cast<void*>(vtable[index]);
    vtable[index] = reinterpret_cast<uintptr_t>(detour);

    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, old_prot, &old_prot);

    return original;
}

}
