#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include "mapper.h"

static bool enable_debug_privilege() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    LUID luid{};
    LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid);

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD err = GetLastError();
    CloseHandle(token);
    return ok && err == ERROR_SUCCESS;
}

static DWORD find_pid(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        printf("usage: injector.exe <dll_path> [process_name]\n");
        return 1;
    }

    const wchar_t* dll_path = argv[1];
    const wchar_t* proc_name = argc >= 3 ? argv[2] : L"dwm.exe";

    wchar_t full_path[MAX_PATH]{};
    GetFullPathNameW(dll_path, MAX_PATH, full_path, nullptr);

    if (GetFileAttributesW(full_path) == INVALID_FILE_ATTRIBUTES) {
        printf("[-] dll not found\n");
        return 1;
    }

    if (!enable_debug_privilege()) {
        printf("[-] run as administrator\n");
        return 1;
    }

    DWORD pid = find_pid(proc_name);
    if (!pid) {
        printf("[-] process not found\n");
        return 1;
    }

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        printf("[-] access denied\n");
        return 1;
    }

    if (mapper::inject(proc, full_path))
        printf("[+] ok\n");
    else
        printf("[-] failed\n");

    CloseHandle(proc);
    return 0;
}
