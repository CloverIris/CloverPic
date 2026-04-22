#include "Render/FontManager.h"
#include "Render/RenderBackend.h"
#include <dwrite.h>
#include <algorithm>

namespace VividPic {

FontManager& FontManager::GetInstance() {
    static FontManager instance;
    return instance;
}

void FontManager::Initialize() {
    if (m_initialized) return;
    
    auto& backend = Render::RenderBackend::GetInstance();
    IDWriteFactory* factory = backend.GetWriteFactory();
    if (!factory) return;
    
    IDWriteFontCollection* collection = nullptr;
    HRESULT hr = factory->GetSystemFontCollection(&collection, FALSE);
    if (FAILED(hr) || !collection) return;
    
    uint32_t count = collection->GetFontFamilyCount();
    for (uint32_t i = 0; i < count; ++i) {
        IDWriteFontFamily* family = nullptr;
        hr = collection->GetFontFamily(i, &family);
        if (FAILED(hr) || !family) continue;
        
        IDWriteLocalizedStrings* names = nullptr;
        hr = family->GetFamilyNames(&names);
        if (SUCCEEDED(hr) && names) {
            uint32_t nameLen = 0;
            hr = names->GetStringLength(0, &nameLen);
            if (SUCCEEDED(hr)) {
                std::wstring name;
                name.resize(nameLen + 1);
                hr = names->GetString(0, name.data(), nameLen + 1);
                if (SUCCEEDED(hr)) {
                    name.resize(nameLen);
                    if (!name.empty()) {
                        m_fontFamilies.push_back(name);
                    }
                }
            }
            names->Release();
        }
        family->Release();
    }
    
    collection->Release();
    
    // Sort alphabetically
    std::sort(m_fontFamilies.begin(), m_fontFamilies.end());
    
    m_initialized = true;
}

bool FontManager::HasFont(const std::wstring& family) const {
    return std::find(m_fontFamilies.begin(), m_fontFamilies.end(), family) != m_fontFamilies.end();
}

} // namespace VividPic
