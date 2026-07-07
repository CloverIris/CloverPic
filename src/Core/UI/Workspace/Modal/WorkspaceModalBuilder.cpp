#include "Core/UI/Workspace/Modal/WorkspaceModalBuilder.h"
#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"

namespace CloverPic::Core {

namespace {

void BuildCanvasSizeModal(const Size& viewport,
                          const Rect& fullRect,
                          const WorkspaceEditorFacade& editor,
                          const WorkspaceCanvasSizeState& canvasSizeState,
                          CoreUI::UiScene& scene) {
    CoreUI::UiNode blocker;
    blocker.type = CoreUI::UiNodeType::Panel;
    blocker.bounds = fullRect;
    blocker.command = static_cast<uint32_t>(AppCommand::None);
    blocker.zOrder = 90;
    blocker.flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive;
    blocker.accessibilityLabel = L"Canvas size modal blocker";
    const auto blockerId = scene.AddNode(std::move(blocker));
    scene.PushModal(blockerId);

    const int modalW = 660;
    const int modalH = 392;
    const int left = std::max(20, viewport.width / 2 - modalW / 2);
    const int top = std::max(20, viewport.height / 2 - modalH / 2);

    auto addNode = [&](CoreUI::UiNodeType type, const Rect& rect, String label, AppCommand command = AppCommand::None,
                       uint64_t userData = 0, String payload = L"", bool interactive = true) {
        CoreUI::UiNode node;
        node.parentId = blockerId;
        node.type = type;
        node.bounds = rect;
        node.label = std::move(label);
        node.accessibilityLabel = node.label;
        node.command = static_cast<uint32_t>(command);
        node.userData = userData;
        node.payload = std::move(payload);
        node.zOrder = 100;
        node.flags = Presentation::UiNodeVisible | (interactive ? static_cast<uint32_t>(Presentation::UiNodeInteractive) : 0u);
        return scene.AddNode(std::move(node));
    };

    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 50, left + 200, top + 72),
            L"Current canvas size", AppCommand::None, 0, L"", false);
    if (auto project = editor.GetProject()) {
        std::wstringstream summary;
        summary << project->GetCanvas().widthPx << L" x " << project->GetCanvas().heightPx << L" px";
        addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 74, left + 220, top + 96),
                summary.str(), AppCommand::None, 0, L"", false);
    }

    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 112, left + 150, top + 134), L"Width", AppCommand::None, 0, L"", false);
    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 160, left + 150, top + 182), L"Height", AppCommand::None, 0, L"", false);
    const auto widthId = addNode(CoreUI::UiNodeType::SearchBox, Rect(left + 132, top + 102, left + 252, top + 138),
                                 std::to_wstring(canvasSizeState.width), AppCommand::None, 0, WorkspaceCanvasSizeWidthPayload);
    const auto heightId = addNode(CoreUI::UiNodeType::SearchBox, Rect(left + 132, top + 150, left + 252, top + 186),
                                  std::to_wstring(canvasSizeState.height), AppCommand::None, 0, WorkspaceCanvasSizeHeightPayload);

    addNode(CoreUI::UiNodeType::Button, Rect(left + 268, top + 102, left + 354, top + 138), L"Swap",
            AppCommand::SwapCanvasOrientation);
    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 206, left + 150, top + 228), L"Anchor", AppCommand::None, 0, L"", false);
    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 230, left + 320, top + 250),
            L"Choose what stays fixed while resizing.", AppCommand::None, 0, L"", false);

    const int anchorLeft = left + 132;
    const int anchorTop = top + 236;
    for (uint32_t row = 0; row < 3; ++row) {
        for (uint32_t col = 0; col < 3; ++col) {
            const uint32_t index = row * 3 + col;
            const String label = (canvasSizeState.anchor == static_cast<WorkspaceCanvasAnchor>(index)) ? L"*" : L"";
            addNode(CoreUI::UiNodeType::Button,
                    Rect(anchorLeft + static_cast<int>(col) * 32, anchorTop + static_cast<int>(row) * 32,
                         anchorLeft + static_cast<int>(col) * 32 + 28, anchorTop + static_cast<int>(row) * 32 + 28),
                    label, AppCommand::SetCanvasAnchor, index, WorkspaceCanvasAnchorPayload);
        }
    }

    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 332, left + 210, top + 350), L"Quick presets", AppCommand::None, 0, L"", false);
    addNode(CoreUI::UiNodeType::Button, Rect(left + 132, top + 350, left + 252, top + 382), L"1600 x 1000", AppCommand::CreateCanvasPreset, PackCanvasPreset(1600, 1000));
    addNode(CoreUI::UiNodeType::Button, Rect(left + 258, top + 350, left + 378, top + 382), L"1920 x 1080", AppCommand::CreateCanvasPreset, PackCanvasPreset(1920, 1080));
    addNode(CoreUI::UiNodeType::Button, Rect(left + 384, top + 350, left + 504, top + 382), L"A4 2480 x 3508", AppCommand::CreateCanvasPreset, PackCanvasPreset(2480, 3508));

    addNode(CoreUI::UiNodeType::Button, Rect(left + 524, top + 348, left + 588, top + 382), L"Apply",
            AppCommand::CreateCanvasFromForm);
    addNode(CoreUI::UiNodeType::Button, Rect(left + 594, top + 348, left + 658, top + 382), L"Cancel",
            AppCommand::CloseModal);

    if (canvasSizeState.focusedField == WorkspaceCanvasSizeField::Width) {
        scene.SetFocus(widthId);
    } else if (canvasSizeState.focusedField == WorkspaceCanvasSizeField::Height) {
        scene.SetFocus(heightId);
    }
}

} // namespace

void WorkspaceModalBuilder::Build(ModalKind kind,
                                  const Size& viewport,
                                  const Rect& fullRect,
                                  const WorkspaceEditorFacade& editor,
                                  const WorkspaceCanvasSizeState& canvasSizeState,
                                  CoreUI::UiScene& scene) {
    if (kind == ModalKind::CanvasSize) {
        BuildCanvasSizeModal(viewport, fullRect, editor, canvasSizeState, scene);
    }
}

} // namespace CloverPic::Core
