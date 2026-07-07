#include "Core/UI/ProjectManager/Interaction/ProjectManagerPointerInputController.h"

namespace CloverPic::Core {

void ProjectManagerPointerInputController::HandlePointer(const Input::PointerEvent& event,
                                                         ProjectManagerPointerInputContext& context,
                                                         const ProjectManagerPointerInputCallbacks& callbacks) {
    if (!context.scene || !context.uiState) {
        return;
    }

    auto* hit = context.scene->HitTest(event.position);
    const Presentation::UiNodeId hitId = hit ? hit->id : 0;
    if (event.action == Input::PointerAction::Move && hitId != context.scene->GetHover()) {
        context.scene->SetHover(hitId);
        callbacks.markDirty(context.fullRect);
    }

    if (event.action == Input::PointerAction::Down) {
        context.scene->SetCapture(hitId);
        context.scene->SetFocus(hitId);
        if (hit && hit->type == CoreUI::UiNodeType::SearchBox) {
            if (hit->payload == ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Width)) {
                context.uiState->activeField = ProjectManagerActiveField::Width;
            } else if (hit->payload == ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Height)) {
                context.uiState->activeField = ProjectManagerActiveField::Height;
            } else if (hit->payload == ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Dpi)) {
                context.uiState->activeField = ProjectManagerActiveField::Dpi;
            } else {
                context.uiState->activeField = ProjectManagerActiveField::Search;
            }
        } else {
            context.uiState->activeField = ProjectManagerActiveField::None;
        }
        callbacks.markDirty(context.fullRect);
        return;
    }

    if (event.action == Input::PointerAction::Up) {
        if (hit && hit->id == context.scene->GetCapture()) {
            callbacks.dispatch(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
            callbacks.rebuildScene();
        }
        context.scene->SetCapture(0);
        callbacks.markDirty(context.fullRect);
    }
}

} // namespace CloverPic::Core
