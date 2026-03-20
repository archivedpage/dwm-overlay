#include <Windows.h>
#include <atlbase.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include "dxgi_undoc.h"
#include "vthook.h"
#include "render.h"
#include "input.h"

static fn_PresentDWM_t o_present_dwm = nullptr;
static fn_PresentMPO_t o_present_mpo = nullptr;
static RECT g_fullscreen_rect = {};

static HRESULT STDMETHODCALLTYPE hk_present_dwm(
    IDXGISwapChainDWMLegacy* sc,
    UINT sync, UINT flags,
    UINT dirty_count, const RECT* dirty_rects,
    UINT scroll_count, const DXGI_SCROLL_RECT* scroll_rects,
    IDXGIResource* resource, UINT frame_idx)
{
    if (!(flags & DXGI_PRESENT_TEST))
        render::on_present(sc);

    return o_present_dwm(sc, sync, flags, 1, &g_fullscreen_rect,
        0, nullptr, resource, frame_idx);
}

static HRESULT STDMETHODCALLTYPE hk_present_mpo(
    IDXGISwapChainDWMLegacy* sc,
    UINT sync, UINT flags,
    DXGI_HDR_METADATA_TYPE meta_type, const VOID* meta,
    UINT overlay_count, const DXGI_PRESENT_MULTIPLANE_OVERLAY* overlays)
{
    if (!(flags & DXGI_PRESENT_TEST))
        render::on_present(sc);

    return o_present_mpo(sc, sync, flags, meta_type, meta, overlay_count, overlays);
}

static DWORD WINAPI init(LPVOID) {
    g_fullscreen_rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    CComPtr<IDXGIFactory> factory;
    if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)))) return 1;

    CComPtr<IDXGIAdapter> adapter;
    if (FAILED(factory->EnumAdapters(0, &adapter))) return 1;

    CComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(0, &output))) return 1;

    CComPtr<IDXGIFactoryDWM> factory_dwm;
    if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory_dwm)))) return 1;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    CComPtr<ID3D11Device> device;
    CComPtr<ID3D11DeviceContext> ctx;
    if (FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_SINGLETHREADED, levels, ARRAYSIZE(levels),
        D3D11_SDK_VERSION, &device, nullptr, &ctx))) return 1;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferCount = 1;

    CComPtr<IDXGISwapChainDWMLegacy> sc;
    if (FAILED(factory_dwm->CreateSwapChain(device, &scd, output, &sc))) return 1;

    o_present_dwm = reinterpret_cast<fn_PresentDWM_t>(
        vthook::swap(sc.p, &hk_present_dwm, 16));
    o_present_mpo = reinterpret_cast<fn_PresentMPO_t>(
        vthook::swap(sc.p, &hk_present_mpo, 23));

    if (!input::install())
        return 1;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    input::uninstall();
    render::shutdown();
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        CreateThread(nullptr, 0, init, nullptr, 0, nullptr);
    return TRUE;
}
