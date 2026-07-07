#include "Core/UI/ProjectManager/Interaction/ProjectManagerKeyInputController.h"
#include <algorithm>
#include <cwctype>

namespace CloverPic::Core {

namespace {

bool IsHomeTextKey(uint32_t keyCode) {
    return (keyCode >= 'A' && keyCode <= 'Z') ||
           (keyCode >= '0' && keyCode <= '9') ||
           keyCode == 0x20 || keyCode == 0xBD || keyCode == 0xBE;
}

wchar_t CharFromKey(uint32_t keyCode, bool shift) {
    if (keyCode >= 'A' && keyCode <= 'Z') {
        const wchar_t ch = static_cast<wchar_t>(keyCode);
        return shift ? ch : static_cast<wchar_t>(std::towlower(ch));
    }
    if (keyCode >= '0' && keyCode <= '9') return static_cast<wchar_t>(keyCode);
    if (keyCode == 0x20) return L' ';
    if (keyCode == 0xBD) return L'-';
    if (keyCode == 0xBE) return L'.';
    return 0;
}

void FocusSearchBox(CoreUI::UiScene& scene, const String& payload) {
    for (const auto& node : scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::SearchBox && node.payload == payload) {
            scene.SetFocus(node.id);
            return;
        }
    }
}

bool HandleFormKey(const Input::KeyEvent& event,
                   ProjectManagerKeyInputContext& context,
                   const ProjectManagerKeyInputCallbacks& callbacks) {
    if (context.uiState->page != ProjectManagerPage::NewImage) {
        return false;
    }
    const auto* focused = context.scene ? context.scene->Find(context.scene->GetFocus()) : nullptr;
    if (!focused || focused->type != CoreUI::UiNodeType::SearchBox) {
        return false;
    }

    ProjectManagerActiveField field = ProjectManagerActiveField::None;
    if (focused->payload == ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Width)) field = ProjectManagerActiveField::Width;
    else if (focused->payload == ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Height)) field = ProjectManagerActiveField::Height;
    else if (focused->payload == ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Dpi)) field = ProjectManagerActiveField::Dpi;
    if (field == ProjectManagerActiveField::None) return false;
    context.uiState->activeField = field;

    auto mutate = [&](uint32_t& value, uint32_t minValue, uint32_t maxValue) {
        String text = std::to_wstring(value);
        if (event.keyCode == 0x08) {
            if (!text.empty()) text.pop_back();
        } else if (event.keyCode >= '0' && event.keyCode <= '9') {
            if (text.size() < 6) text.push_back(static_cast<wchar_t>(event.keyCode));
        } else {
            return false;
        }
        uint32_t next = minValue;
        if (!text.empty()) {
            try {
                next = static_cast<uint32_t>(std::stoul(text));
            } catch (...) {
                next = minValue;
            }
        }
        value = std::clamp(next, minValue, maxValue);
        callbacks.rebuildScene();
        if (context.scene) {
            FocusSearchBox(*context.scene, ProjectManagerUiState::FieldPayload(field));
        }
        callbacks.markDirty(context.fullRect);
        return true;
    };

    if (field == ProjectManagerActiveField::Width) return mutate(context.uiState->formWidth, 16, 65535);
    if (field == ProjectManagerActiveField::Height) return mutate(context.uiState->formHeight, 16, 65535);
    if (field == ProjectManagerActiveField::Dpi) return mutate(context.uiState->formDpi, 1, 2400);
    return false;
}

bool HandleSearchKey(const Input::KeyEvent& event,
                     ProjectManagerKeyInputContext& context,
                     const ProjectManagerKeyInputCallbacks& callbacks) {
    if (context.uiState->page != ProjectManagerPage::Start || !context.scene) {
        return false;
    }
    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    const bool shift = (event.modifiers & Input::ModifierShift) != 0;
    const auto* focused = context.scene->Find(context.scene->GetFocus());
    const bool searchFocused = focused && focused->type == CoreUI::UiNodeType::SearchBox;
    if (event.keyCode == 0x1B && !context.uiState->searchQuery.empty()) {
        context.uiState->searchQuery.clear();
        callbacks.rebuildScene();
        FocusSearchBox(*context.scene, ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Search));
        callbacks.markDirty(context.fullRect);
        return true;
    }
    if (!searchFocused || ctrl) return false;
    if (event.keyCode == 0x08) {
        if (!context.uiState->searchQuery.empty()) {
            context.uiState->searchQuery.pop_back();
            callbacks.rebuildScene();
            FocusSearchBox(*context.scene, ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Search));
            callbacks.markDirty(context.fullRect);
        }
        return true;
    }
    if (IsHomeTextKey(event.keyCode) && context.uiState->searchQuery.size() < 48) {
        const wchar_t ch = CharFromKey(event.keyCode, shift);
        if (ch != 0) {
            context.uiState->searchQuery.push_back(ch);
            callbacks.rebuildScene();
            FocusSearchBox(*context.scene, ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Search));
            callbacks.markDirty(context.fullRect);
            return true;
        }
    }
    return false;
}

} // namespace

void ProjectManagerKeyInputController::HandleKey(const Input::KeyEvent& event,
                                                 ProjectManagerKeyInputContext& context,
                                                 const ProjectManagerKeyInputCallbacks& callbacks) {
    if (event.action != Input::KeyAction::Down || !context.uiState) {
        return;
    }
    if (HandleFormKey(event, context, callbacks)) return;
    if (HandleSearchKey(event, context, callbacks)) return;

    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    if (event.keyCode == 0x1B) {
        if (context.uiState->page != ProjectManagerPage::Start) {
            callbacks.closeModal();
        } else if (context.wantsQuit) {
            *context.wantsQuit = true;
        }
    } else if (event.keyCode == 'N' && ctrl) {
        callbacks.quickNewCanvas();
    } else if (event.keyCode == 'O' && ctrl) {
        callbacks.openProject();
    }
}

} // namespace CloverPic::Core
