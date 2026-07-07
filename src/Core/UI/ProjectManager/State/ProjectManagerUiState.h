#pragma once

#include "Core/Services/CoreServices.h"

namespace CloverPic::Core {

enum class ProjectManagerPage : uint8_t {
    Start,
    NewImage,
    Settings
};

enum class ProjectManagerActiveField : uint8_t {
    None,
    Search,
    Width,
    Height,
    Dpi
};

struct ProjectManagerUiState {
    uint32_t formWidth = 1600;
    uint32_t formHeight = 900;
    uint32_t formDpi = 350;
    bool formTransparent = true;
    Color formBackground = Color(255, 255, 255, 255);
    size_t selectedRgbProfile = 0;
    size_t selectedCmykProfile = 0;
    std::vector<CloverPic::ColorProfileInfo> profiles;
    String searchQuery;
    String settingsStatus = L"Draft settings are local to CloverPic.";
    ProjectManagerPage page = ProjectManagerPage::Start;
    ProjectManagerActiveField activeField = ProjectManagerActiveField::None;
    int settingsCategory = 0;
    bool settingsOpenedFromWorkspace = false;

    static String FieldPayload(ProjectManagerActiveField field);
};

} // namespace CloverPic::Core
