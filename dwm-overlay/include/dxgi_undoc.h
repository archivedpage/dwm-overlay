#pragma once

#include <Windows.h>
#include <dxgi1_5.h>

typedef struct _DXGI_SCROLL_RECT {
    POINT offset;
    RECT region;
} DXGI_SCROLL_RECT;

typedef struct _DXGI_PRESENT_MULTIPLANE_OVERLAY {
    UINT LayerIndex;
    UINT Flags;
    char _pad0[0x14];
    RECT SrcRect;
    RECT DstRect;
    RECT ClipRect;
    DXGI_MODE_ROTATION Rotation;
    char _pad1[0x04];
    UINT DirtyRectsCount;
    RECT* pDirtyRects;
    char _pad2[0x28];
} DXGI_PRESENT_MULTIPLANE_OVERLAY;

MIDL_INTERFACE("f69f223b-45d3-4aa0-98c8-c40c2b231029")
IDXGISwapChainDWMLegacy : public IDXGIDeviceSubObject
{
    virtual HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) = 0;
};

MIDL_INTERFACE("713f394e-92ca-47e7-ab81-1159c2791e54")
IDXGIFactoryDWM : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
        IUnknown* pDevice,
        DXGI_SWAP_CHAIN_DESC* pDesc,
        IDXGIOutput* pOutput,
        IDXGISwapChainDWMLegacy** ppSwapChain
    ) = 0;
};

using fn_PresentDWM_t = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChainDWMLegacy* pSwapChain,
    UINT SyncInterval,
    UINT PresentFlags,
    UINT DirtyRectsCount,
    const RECT* pDirtyRects,
    UINT ScrollRectsCount,
    const DXGI_SCROLL_RECT* pScrollRects,
    IDXGIResource* pResource,
    UINT FrameIndex
);

using fn_PresentMPO_t = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChainDWMLegacy* pSwapChain,
    UINT SyncInterval,
    UINT PresentFlags,
    DXGI_HDR_METADATA_TYPE MetadataType,
    const VOID* pMetadata,
    UINT OverlayCount,
    const DXGI_PRESENT_MULTIPLANE_OVERLAY* pOverlays
);
