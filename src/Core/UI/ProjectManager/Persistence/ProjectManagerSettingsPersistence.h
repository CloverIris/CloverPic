#pragma once

#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"

namespace CloverPic::Core {

class ProjectManagerSettingsPersistence {
public:
    static bool LoadSelectedProfiles(const std::vector<uint8_t>& bytes,
                                     ProjectManagerUiState& uiState,
                                     String& statusText);

    static void SaveSelectedProfiles(std::vector<uint8_t>& bytes, const ProjectManagerUiState& uiState);
};

} // namespace CloverPic::Core
