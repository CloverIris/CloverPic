#pragma once

#include "Utils/Types.h"
#include <vector>

namespace VividPic {

class FontManager {
public:
    static FontManager& GetInstance();
    
    void Initialize();
    bool IsInitialized() const { return m_initialized; }
    
    const std::vector<std::wstring>& GetFontFamilies() const { return m_fontFamilies; }
    bool HasFont(const std::wstring& family) const;
    
private:
    FontManager() = default;
    
    bool m_initialized = false;
    std::vector<std::wstring> m_fontFamilies;
};

} // namespace VividPic
