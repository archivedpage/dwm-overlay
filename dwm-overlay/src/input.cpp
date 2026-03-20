#include "input.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static HHOOK g_mouse_hook = nullptr;
static HHOOK g_kb_hook = nullptr;

static LRESULT CALLBACK mouse_proc(int code, WPARAM wp, LPARAM lp) {
    auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplWin32_WndProcHandler(
            GetDesktopWindow(),
            static_cast<UINT>(wp),
            info->flags,
            MAKELPARAM(info->pt.x, info->pt.y)
        );

        if (ImGui::GetIO().WantCaptureMouse && wp != WM_MOUSEMOVE)
            return -1;
    }

    return CallNextHookEx(nullptr, code, wp, lp);
}

static LRESULT CALLBACK keyboard_proc(int code, WPARAM wp, LPARAM lp) {
    auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplWin32_WndProcHandler(
            GetDesktopWindow(),
            static_cast<UINT>(wp),
            info->vkCode,
            info->scanCode
        );

        if (ImGui::GetIO().WantCaptureKeyboard)
            return -1;
    }

    if (info->vkCode == VK_END)
        TerminateProcess(GetCurrentProcess(), 0);

    return CallNextHookEx(nullptr, code, wp, lp);
}

bool input::install() {
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, nullptr, 0);
    g_kb_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, nullptr, 0);
    return g_mouse_hook && g_kb_hook;
}

void input::uninstall() {
    if (g_mouse_hook) UnhookWindowsHookEx(g_mouse_hook);
    if (g_kb_hook) UnhookWindowsHookEx(g_kb_hook);
    g_mouse_hook = nullptr;
    g_kb_hook = nullptr;
}
