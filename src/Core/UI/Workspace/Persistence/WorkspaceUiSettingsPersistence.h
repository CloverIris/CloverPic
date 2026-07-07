#pragma once

#include "Core/UI/Workspace/WorkspaceUiState.h"
#include <vector>

namespace CloverPic::Core {

class WorkspaceUiSettingsPersistence {
public:
    static constexpr const char* Magic() { return "CLVPIC_WORKSPACE_UI_V2"; }

    static void LoadFromBytes(const std::vector<uint8_t>& bytes, WorkspaceUiState& uiState);
    static void SaveToBytes(std::vector<uint8_t>& bytes, const WorkspaceUiState& uiState);
};

} // namespace CloverPic::Core
