#include "Core/UI/ProjectManager/Persistence/ProjectManagerSettingsPersistence.h"
#include <cstring>
#include <sstream>

namespace CloverPic::Core {

bool ProjectManagerSettingsPersistence::LoadSelectedProfiles(const std::vector<uint8_t>& bytes,
                                                             ProjectManagerUiState& uiState,
                                                             String& statusText) {
    if (bytes.empty()) {
        return false;
    }

    size_t offset = 0;
    auto readString = [&](String& outValue) -> bool {
        if (offset + sizeof(uint32_t) > bytes.size()) {
            return false;
        }
        uint32_t len = 0;
        std::memcpy(&len, bytes.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (offset + static_cast<size_t>(len) * sizeof(uint32_t) > bytes.size()) {
            return false;
        }
        outValue.clear();
        outValue.reserve(len);
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t cp = 0;
            std::memcpy(&cp, bytes.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            outValue.push_back(static_cast<wchar_t>(cp));
        }
        return true;
    };

    String rgbPath;
    String cmykPath;
    if (!readString(rgbPath) || !readString(cmykPath)) {
        statusText = L"Stored settings could not be read. Using defaults.";
        return false;
    }

    for (size_t i = 0; i < uiState.profiles.size(); ++i) {
        if (!rgbPath.empty() && uiState.profiles[i].path == rgbPath) {
            uiState.selectedRgbProfile = i;
        }
        if (!cmykPath.empty() && uiState.profiles[i].path == cmykPath) {
            uiState.selectedCmykProfile = i;
        }
    }
    if (!rgbPath.empty() || !cmykPath.empty()) {
        statusText = L"Loaded saved settings.";
    }
    return true;
}

void ProjectManagerSettingsPersistence::SaveSelectedProfiles(std::vector<uint8_t>& bytes,
                                                             const ProjectManagerUiState& uiState) {
    std::ostringstream stream(std::ios::binary);
    const String rgb = uiState.selectedRgbProfile < uiState.profiles.size() ? uiState.profiles[uiState.selectedRgbProfile].path : L"";
    const String cmyk = uiState.selectedCmykProfile < uiState.profiles.size() ? uiState.profiles[uiState.selectedCmykProfile].path : L"";
    auto writeString = [&](const String& value) {
        const uint32_t len = static_cast<uint32_t>(value.size());
        stream.write(reinterpret_cast<const char*>(&len), sizeof(len));
        for (const wchar_t ch : value) {
            const uint32_t cp = static_cast<uint32_t>(ch);
            stream.write(reinterpret_cast<const char*>(&cp), sizeof(cp));
        }
    };
    writeString(rgb);
    writeString(cmyk);
    const std::string data = stream.str();
    bytes.assign(data.begin(), data.end());
}

} // namespace CloverPic::Core
