#include "Platform/Windows/RenderBackend.h"

namespace CloverPic {
namespace Render {

RenderBackend& RenderBackend::GetInstance() {
    static RenderBackend instance;
    return instance;
}

RenderBackend::~RenderBackend() {
    Shutdown();
}

bool RenderBackend::Initialize() {
    if (m_wicFactory) return true;

    const HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_wicFactory)
    );
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }

    return true;
}

void RenderBackend::Shutdown() {
    if (m_wicFactory) {
        m_wicFactory->Release();
        m_wicFactory = nullptr;
    }
}

} // namespace Render
} // namespace CloverPic
