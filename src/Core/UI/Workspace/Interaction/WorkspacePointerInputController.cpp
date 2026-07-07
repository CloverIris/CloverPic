#include "Core/UI/Workspace/Interaction/WorkspacePointerInputController.h"

namespace CloverPic::Core {

namespace {

WorkspaceInteractionCallbacks MakeInteractionCallbacks(const WorkspacePointerInputCallbacks& callbacks) {
    return WorkspaceInteractionCallbacks{
        callbacks.dispatch,
        callbacks.markDirty,
        callbacks.markProjectDirty,
        callbacks.rebuildScene,
        callbacks.setStatus
    };
}

void UpdateCanvasInteractionState(WorkspacePointerInputContext& context,
                                  const Input::PointerEvent& event,
                                  const WorkspacePointerInputCallbacks& callbacks) {
    if (!context.editor || !context.scheduler) {
        return;
    }

    const bool changed = context.editor->HandleCanvasPointer(event, context.canvasRect);
    if (changed) {
        callbacks.markProjectDirty();
    }
    if (context.editor->IsInteracting()) {
        context.scheduler->SetAnimatedRegion(context.canvasAnimationRegionId, context.canvasRect, 60);
    } else {
        context.scheduler->ClearAnimatedRegion(context.canvasAnimationRegionId);
    }
    callbacks.markDirty(context.canvasRect);
}

bool CanRouteToCanvas(const WorkspacePointerInputContext& context, const CoreUI::UiNode* hit, const Point& position) {
    return !hit && context.modalKind == ModalKind::None && context.openMenu && context.openMenu->empty() &&
           context.canvasRect.Contains(position);
}

bool KeepsLayerBlendDropdownOpen(const CoreUI::UiNode* hit) {
    if (!hit) {
        return false;
    }
    const auto command = static_cast<AppCommand>(hit->command);
    return command == AppCommand::ToggleLayerBlendDropdown || command == AppCommand::SetBlendMode;
}

} // namespace

void WorkspacePointerInputController::HandlePointer(const Input::PointerEvent& event,
                                                    WorkspacePointerInputContext& context,
                                                    const WorkspacePointerInputCallbacks& callbacks) {
    if (!context.scene || !context.uiState || !context.layout || !context.interaction || !context.editor) {
        return;
    }

    auto* hit = context.scene->HitTest(event.position);
    Presentation::UiNodeId hitId = hit ? hit->id : 0;
    const WorkspaceInteractionCallbacks interactionCallbacks = MakeInteractionCallbacks(callbacks);

    if ((event.action == Input::PointerAction::Move || event.action == Input::PointerAction::Up) &&
        context.interaction->Active().kind != ActiveWorkspaceInteractionKind::None) {
        const bool panelDrag = context.interaction->Active().kind == ActiveWorkspaceInteractionKind::PanelDrag;
        if (event.action == Input::PointerAction::Move) {
            context.interaction->Update(event, *context.scene, *context.uiState, *context.layout, *context.editor, interactionCallbacks);
        } else {
            context.interaction->End(event, *context.scene, *context.uiState, *context.layout, *context.editor, interactionCallbacks);
            if (panelDrag) {
                callbacks.saveWorkspaceUiSettings();
            }
        }
        return;
    }

    if (event.action == Input::PointerAction::Move && hitId != context.scene->GetHover()) {
        context.scene->SetHover(hitId);
        callbacks.markDirty(context.fullRect);
    }

    if (event.action == Input::PointerAction::Down) {
        if (context.uiState->layerBlendDropdownOpen && !KeepsLayerBlendDropdownOpen(hit)) {
            context.uiState->layerBlendDropdownOpen = false;
            callbacks.rebuildScene();
            hit = context.scene->HitTest(event.position);
            hitId = hit ? hit->id : 0;
            callbacks.markDirty(context.fullRect);
        }

        if (hit && context.interaction->Begin(event, *hit, *context.scene, *context.uiState, *context.layout, *context.editor, interactionCallbacks)) {
            return;
        }

        context.scene->SetCapture(hitId);
        context.scene->SetFocus(hitId);
        if (hit && hit->type == CoreUI::UiNodeType::SearchBox && context.canvasSizeState) {
            if (hit->payload == WorkspaceCanvasSizeWidthPayload) {
                context.canvasSizeState->focusedField = WorkspaceCanvasSizeField::Width;
            } else if (hit->payload == WorkspaceCanvasSizeHeightPayload) {
                context.canvasSizeState->focusedField = WorkspaceCanvasSizeField::Height;
            }
        }
        callbacks.markDirty(context.fullRect);

        if (CanRouteToCanvas(context, hit, event.position)) {
            UpdateCanvasInteractionState(context, event, callbacks);
        }
        return;
    }

    if (event.action == Input::PointerAction::Up) {
        if (hit && hit->id == context.scene->GetCapture()) {
            if (hit->type == CoreUI::UiNodeType::MenuHeader) {
                if (context.openMenu) {
                    *context.openMenu = (*context.openMenu == hit->payload) ? L"" : hit->payload;
                    context.uiState->openMenu = *context.openMenu;
                }
                callbacks.rebuildScene();
            } else if (hit->type == CoreUI::UiNodeType::MenuItem) {
                callbacks.dispatch(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
                if (context.openMenu) {
                    context.openMenu->clear();
                    context.uiState->openMenu.clear();
                }
                callbacks.rebuildScene();
            } else {
                callbacks.dispatch(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
            }
        } else if (context.openMenu && !context.openMenu->empty()) {
            context.openMenu->clear();
            context.uiState->openMenu.clear();
            callbacks.rebuildScene();
        }
        context.scene->SetCapture(0);
        callbacks.markDirty(context.fullRect);
        return;
    }

    if (CanRouteToCanvas(context, hit, event.position)) {
        UpdateCanvasInteractionState(context, event, callbacks);
    }
}

} // namespace CloverPic::Core
