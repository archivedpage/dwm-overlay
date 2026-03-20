#include "render.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <atlbase.h>

static CComPtr<ID3D11Device> g_device;
static CComPtr<ID3D11DeviceContext> g_ctx;
static CComPtr<ID3D11RenderTargetView> g_rtv;
static IDXGISwapChainDWMLegacy* g_target_sc = nullptr;
static bool g_show_menu = true;

static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.ChildRounding = 4.0f;
    s.PopupRounding = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.TabRounding = 4.0f;
    s.WindowPadding = ImVec2(12, 12);
    s.FramePadding = ImVec2(8, 4);
    s.ItemSpacing = ImVec2(8, 6);

    auto& c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.10f, 0.94f);
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.34f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.40f, 0.20f, 0.80f, 0.70f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.30f, 0.90f, 0.80f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.40f, 1.00f, 0.90f);
    c[ImGuiCol_CheckMark] = ImVec4(0.65f, 0.40f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.30f, 0.90f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.65f, 0.40f, 1.00f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.40f, 0.20f, 0.80f, 0.50f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.30f, 0.90f, 0.60f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.40f, 1.00f, 0.70f);
    c[ImGuiCol_Separator] = ImVec4(0.30f, 0.20f, 0.50f, 0.50f);
    c[ImGuiCol_Border] = ImVec4(0.30f, 0.20f, 0.50f, 0.40f);
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.96f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.58f, 1.00f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.40f, 0.20f, 0.80f, 0.30f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.30f, 0.90f, 0.50f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.65f, 0.40f, 1.00f, 0.70f);
}

void render::on_present(IDXGISwapChainDWMLegacy* swap_chain) {
    if (!ImGui::GetCurrentContext()) {
        g_target_sc = swap_chain;

        swap_chain->GetDevice(IID_PPV_ARGS(&g_device));
        g_device->GetImmediateContext(&g_ctx);

        ImGui::CreateContext();
        apply_style();

        ImGui_ImplWin32_Init(GetDesktopWindow());
        ImGui_ImplDX11_Init(g_device, g_ctx);

        CComPtr<ID3D11Texture2D> back_buffer;
        swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        g_device->CreateRenderTargetView(back_buffer, nullptr, &g_rtv);
    }

    if (swap_chain != g_target_sc)
        return;

    g_ctx->OMSetRenderTargets(1, &g_rtv.p, nullptr);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_show_menu) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("dwm-overlay", &g_show_menu, ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(ImVec4(0.65f, 0.40f, 1.00f, 1.00f), "overlay");
        ImGui::Separator();

        static bool demo = false;
        ImGui::Checkbox("Show Demo Window", &demo);

        if (demo)
            ImGui::ShowDemoWindow(&demo);

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void render::shutdown() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    g_rtv.Release();
    g_ctx.Release();
    g_device.Release();
}
