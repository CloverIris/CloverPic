#include "Render/BrushPresetManager.h"
#include "Render/BrushEngine.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>

namespace VividPic {
namespace Render {

BrushPresetManager& BrushPresetManager::GetInstance() {
    static BrushPresetManager instance;
    return instance;
}

void BrushPresetManager::Initialize() {
    if (m_initialized) return;

    // Built-in preset 1: Pen (钢笔)
    m_presets.push_back({ L"钢笔", BrushTipType::RoundHard, 8.0f, 1.0f, 1.0f, 0.15f, 0.0f, 0, Color::FromHex(0x000000), 1.0f });

    // Built-in preset 2: G-Pen (G笔尖)
    m_presets.push_back({ L"G笔尖", BrushTipType::RoundHard, 10.0f, 1.0f, 1.0f, 0.10f, 0.0f, 0, Color::FromHex(0x000000), 1.0f });

    // Built-in preset 3: G-Pen Soft (G笔尖软)
    m_presets.push_back({ L"G笔尖(软)", BrushTipType::RoundSoft, 10.0f, 0.8f, 0.9f, 0.10f, 0.0f, 0, Color::FromHex(0x000000), 1.0f });

    // Built-in preset 4: Ink Brush (毛笔水墨)
    m_presets.push_back({ L"毛笔(水墨)", BrushTipType::Bristle, 30.0f, 0.6f, 0.4f, 0.25f, 0.3f, 1, Color::FromHex(0x1a1a1a), 1.0f });

    // Built-in preset 5: Airbrush (喷枪)
    m_presets.push_back({ L"喷枪", BrushTipType::RoundSoft, 100.0f, 0.3f, 0.2f, 0.80f, 0.0f, 1, Color::FromHex(0x000000), 1.0f });

    m_initialized = true;
}

const BrushPreset& BrushPresetManager::GetPreset(size_t index) const {
    static BrushPreset empty;
    if (index >= m_presets.size()) return empty;
    return m_presets[index];
}

void BrushPresetManager::ApplyPreset(size_t index) {
    if (index >= m_presets.size()) return;
    const auto& p = m_presets[index];
    auto& engine = BrushEngine::GetInstance();
    engine.SetSize(p.size);
    engine.SetOpacity(p.opacity);
    engine.SetFlow(p.flow);
    engine.SetSpacing(p.spacing);
    engine.SetWetMix(p.wetMix);
    engine.SetPressureCurve(p.pressureCurve);
    engine.SetColor(p.color);
    engine.SetTipType(p.tipType);
    engine.SetTextureScale(p.textureScale);
}

void BrushPresetManager::AddPreset(const BrushPreset& preset) {
    m_presets.push_back(preset);
}

void BrushPresetManager::DeletePreset(size_t index) {
    if (index < 5) return; // Protect built-ins
    if (index >= m_presets.size()) return;
    m_presets.erase(m_presets.begin() + index);
}

bool BrushPresetManager::SaveToFile(const std::wstring& path) const {
    int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath;
    if (len > 0) {
        narrowPath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, narrowPath.data(), len, nullptr, nullptr);
        narrowPath.pop_back(); // remove null terminator
    }
    std::ofstream file(narrowPath, std::ios::out);
    if (!file.is_open()) return false;

    file << "; VividPic Brush Preset File\n";
    file << "; Version=1.0\n\n";

    for (size_t i = 5; i < m_presets.size(); ++i) {
        const auto& p = m_presets[i];
        file << "[BrushPreset]\n";
        file << "Name=" << std::string(p.name.begin(), p.name.end()) << "\n";
        file << "TipType=" << static_cast<int>(p.tipType) << "\n";
        file << "Size=" << p.size << "\n";
        file << "Opacity=" << p.opacity << "\n";
        file << "Flow=" << p.flow << "\n";
        file << "Spacing=" << p.spacing << "\n";
        file << "WetMix=" << p.wetMix << "\n";
        file << "PressureCurve=" << p.pressureCurve << "\n";
        file << "TextureScale=" << p.textureScale << "\n";
        file << "\n";
    }

    return true;
}

bool BrushPresetManager::LoadFromFile(const std::wstring& path) {
    int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath;
    if (len > 0) {
        narrowPath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, narrowPath.data(), len, nullptr, nullptr);
        narrowPath.pop_back();
    }
    std::ifstream file(narrowPath, std::ios::in);
    if (!file.is_open()) return false;

    // Stub: full parser in M6
    return true;
}

std::wstring BrushPresetManager::GetDefaultSavePath() const {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::wstring fullPath = path;
        fullPath += L"\\VividPic\\brushes\\presets.vvpbrush";
        return fullPath;
    }
    return L"";
}

} // namespace Render
} // namespace VividPic
