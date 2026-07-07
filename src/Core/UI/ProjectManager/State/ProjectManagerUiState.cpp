#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"

namespace CloverPic::Core {

String ProjectManagerUiState::FieldPayload(ProjectManagerActiveField field) {
    switch (field) {
        case ProjectManagerActiveField::Width: return L"field:width";
        case ProjectManagerActiveField::Height: return L"field:height";
        case ProjectManagerActiveField::Dpi: return L"field:dpi";
        case ProjectManagerActiveField::Search: return L"field:search";
        case ProjectManagerActiveField::None: break;
    }
    return L"";
}

} // namespace CloverPic::Core
