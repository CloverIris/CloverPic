#pragma once

#include "Render/BrushPreset.h"
#include <vector>
#include <string>

namespace VividPic {
namespace Render {

class BrushPresetManager {
public:
    static BrushPresetManager& GetInstance();

    // Initialize with built-in presets
    void Initialize();

    // Get all presets
    const std::vector<BrushPreset>& GetPresets() const { return m_presets; }

    // Get preset by index
    const BrushPreset& GetPreset(size_t index) const;

    // Apply preset to BrushEngine
    void ApplyPreset(size_t index);

    // Add custom preset
    void AddPreset(const BrushPreset& preset);

    // Delete preset (cannot delete built-ins index 0-4)
    void DeletePreset(size_t index);

    // Serialization (stub for M4, full in M6)
    bool SaveToFile(const std::wstring& path) const;
    bool LoadFromFile(const std::wstring& path);

    // Default save path: %APPDATA%/VividPic/brushes/presets.vvpbrush
    std::wstring GetDefaultSavePath() const;

private:
    BrushPresetManager() = default;

    std::vector<BrushPreset> m_presets;
    bool m_initialized = false;
};

} // namespace Render
} // namespace VividPic
