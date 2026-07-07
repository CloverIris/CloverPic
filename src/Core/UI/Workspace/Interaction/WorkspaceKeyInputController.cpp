#include "Core/UI/Workspace/Interaction/WorkspaceKeyInputController.h"
#include <algorithm>

namespace CloverPic::Core {

namespace {

constexpr uint32_t KeyBracketLeft = 0xDB;
constexpr uint32_t KeyBracketRight = 0xDD;

bool HandleCanvasSizeKey(const Input::KeyEvent& event,
                         WorkspaceCanvasSizeState& state,
                         const WorkspaceKeyInputCallbacks& callbacks) {
    if (event.keyCode == 0x1B) {
        callbacks.closeModal();
        return true;
    }
    if (event.keyCode == 0x0D) {
        callbacks.applyCanvasSize();
        return true;
    }
    if (event.keyCode == 0x09) {
        state.focusedField = state.focusedField == WorkspaceCanvasSizeField::Height
            ? WorkspaceCanvasSizeField::Width
            : WorkspaceCanvasSizeField::Height;
        callbacks.touchWorkspace();
        return true;
    }
    if (state.focusedField == WorkspaceCanvasSizeField::None) {
        return false;
    }

    auto* value = state.focusedField == WorkspaceCanvasSizeField::Width ? &state.width : &state.height;
    if (event.keyCode >= '0' && event.keyCode <= '9') {
        const uint32_t digit = static_cast<uint32_t>(event.keyCode - '0');
        *value = std::min<uint32_t>(*value * 10 + digit, 65535u);
        callbacks.touchWorkspace();
        return true;
    }
    if (event.keyCode == 0x08) {
        *value /= 10;
        callbacks.touchWorkspace();
        return true;
    }
    if (event.keyCode == 0x26) {
        *value = std::min<uint32_t>(*value + 1, 65535u);
        callbacks.touchWorkspace();
        return true;
    }
    if (event.keyCode == 0x28) {
        *value = *value > 1 ? *value - 1 : 1;
        callbacks.touchWorkspace();
        return true;
    }
    return false;
}

} // namespace

bool WorkspaceKeyInputController::HandleKeyDown(const Input::KeyEvent& event,
                                                WorkspaceKeyInputContext& context,
                                                const WorkspaceKeyInputCallbacks& callbacks) {
    if (event.action != Input::KeyAction::Down) {
        return false;
    }

    if (context.modalKind == ModalKind::CanvasSize && context.canvasSizeState &&
        HandleCanvasSizeKey(event, *context.canvasSizeState, callbacks)) {
        return true;
    }

    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    const bool shift = (event.modifiers & Input::ModifierShift) != 0;

    if (event.keyCode == 0x1B) {
        if (context.openMenu && !context.openMenu->empty()) {
            context.openMenu->clear();
            callbacks.touchWorkspace();
        } else if (context.modalKind != ModalKind::None) {
            callbacks.closeModal();
        } else if (context.wantsClose) {
            *context.wantsClose = true;
        }
        return true;
    }
    if (event.keyCode == 'S' && ctrl) {
        callbacks.saveProject(shift);
        return true;
    }
    if (event.keyCode == 'Z' && ctrl) {
        callbacks.undo();
        return true;
    }
    if (event.keyCode == 'Y' && ctrl) {
        callbacks.redo();
        return true;
    }
    if (event.keyCode == 'A' && ctrl) {
        callbacks.selectAll();
        return true;
    }
    if (event.keyCode == 'D' && ctrl) {
        callbacks.clearSelection();
        return true;
    }
    if (event.keyCode == 'I' && ctrl && shift) {
        callbacks.invertSelection();
        return true;
    }
    if (event.keyCode == '0' && ctrl) {
        callbacks.fitCanvas();
        return true;
    }
    if (event.keyCode == '1' && ctrl) {
        callbacks.setZoomPreset(100);
        return true;
    }
    if (event.keyCode == 'K' && ctrl) {
        callbacks.showSettings();
        return true;
    }
    if ((event.keyCode == 0xBB || event.keyCode == 0x6B) && ctrl) {
        callbacks.zoomIn();
        return true;
    }
    if ((event.keyCode == 0xBD || event.keyCode == 0x6D) && ctrl) {
        callbacks.zoomOut();
        return true;
    }
    if (event.keyCode == 'B') {
        callbacks.switchTool(ToolType::Brush);
        return true;
    }
    if (event.keyCode == 'E') {
        callbacks.switchTool(ToolType::Eraser);
        return true;
    }
    if (event.keyCode == 'V') {
        callbacks.switchTool(ToolType::Move);
        return true;
    }
    if (event.keyCode == 'I') {
        callbacks.switchTool(ToolType::Eyedropper);
        return true;
    }
    if (event.keyCode == 'G') {
        callbacks.switchTool(ToolType::Fill);
        return true;
    }
    if (event.keyCode == 'T') {
        callbacks.setStatus(L"TEXT TOOL NEXT");
        return true;
    }
    if ((event.keyCode == KeyBracketLeft || event.keyCode == KeyBracketRight) && context.editor) {
        const float delta = event.keyCode == KeyBracketLeft ? -2.0f : 2.0f;
        context.editor->SetBrushSize(std::clamp(context.editor->GetBrushSize() + delta, 1.0f, 5000.0f));
        callbacks.touchWorkspace();
        return true;
    }

    return false;
}

} // namespace CloverPic::Core
