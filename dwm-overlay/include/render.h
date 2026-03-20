#pragma once

#include "dxgi_undoc.h"
#include <d3d11.h>

namespace render {

void on_present(IDXGISwapChainDWMLegacy* swap_chain);
void shutdown();

}
