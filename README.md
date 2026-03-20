# dwm-overlay

dwm overlay for Windows 10/11 renders ImGui directly inside the DWM compositing using undocumented DXGI interfaces no transparent windows, no pattern scanning.

## Technical Details

- DWM swap chain `IDXGIFactoryDWM::CreateSwapChain` using GUID `713f394e-92ca-47e7-ab81-1159c2791e54`
- Static CRT linking (`/MT`) - no runtime DLL dependencies

## known errors

- Vtable offsets (16, 23) are for Windows 10/11 - older versions may require adjustment
- Windows 11 24H2+ with NVIDIA drivers may have compatibility issues
