#pragma once

#include <windows.h>
#include <wincodec.h>

namespace CloverPic {
namespace Render {

class RenderBackend {
public:
    static RenderBackend& GetInstance();
    
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_wicFactory != nullptr; }
    
    IWICImagingFactory* GetWicFactory() const { return m_wicFactory; }

private:
    RenderBackend() = default;
    ~RenderBackend();
    
    IWICImagingFactory* m_wicFactory = nullptr;
};

} // namespace Render
} // namespace CloverPic
