#include "Core/UI/ProjectManager/Components/ProjectManagerComponentBuilder.h"
#include <algorithm>

namespace CloverPic::Core {

namespace {

Presentation::UiNodeId AddNode(CoreUI::UiScene& scene,
                               CoreUI::UiNodeType type,
                               const Rect& bounds,
                               String label,
                               AppCommand command = AppCommand::None,
                               uint64_t userData = 0,
                               String payload = L"",
                               int z = 20,
                               uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                               Presentation::IconId iconId = Presentation::IconId::None,
                               CoreUI::IconPlacement placement = CoreUI::IconPlacement::None) {
    CoreUI::UiNode node;
    node.type = type;
    node.bounds = bounds;
    node.label = std::move(label);
    node.accessibilityLabel = node.label;
    node.command = static_cast<uint32_t>(command);
    node.userData = userData;
    node.payload = std::move(payload);
    node.zOrder = z;
    node.flags = flags;
    node.iconId = iconId;
    node.iconPlacement = placement;
    return scene.AddNode(std::move(node));
}

void BuildNewImageScene(const Size& viewport,
                        const ProjectManagerUiState& uiState,
                        CoreUI::UiScene& scene) {
    const ProjectManagerLayout layout = ProjectManagerLayoutComputer::Compute(viewport, ProjectManagerPage::NewImage);
    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(layout.left, layout.top + 70, layout.left + layout.navWidth, layout.top + 114),
            L"RECENT", AppCommand::CloseModal);
    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(layout.left, layout.top + 122, layout.left + layout.navWidth, layout.top + 166),
            L"CREATE NEW IMAGE", AppCommand::ShowNewCanvasModal);
    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(layout.left, layout.top + 174, layout.left + layout.navWidth, layout.top + 218),
            L"SETTINGS", AppCommand::ShowSettingsModal);
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 36, layout.top, layout.contentRight, layout.top + 30),
            L"X", AppCommand::CloseModal, 0, L"", 60, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::X, CoreUI::IconPlacement::Center);

    auto field = [&](int y, const String& label, const String& value, ProjectManagerActiveField fieldId) {
        AddNode(scene, CoreUI::UiNodeType::Text, Rect(layout.contentLeft, y + 8, layout.contentLeft + layout.labelWidth, y + layout.rowHeight),
                label, AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
        AddNode(scene, CoreUI::UiNodeType::SearchBox, Rect(layout.fieldLeft, y, layout.fieldLeft + layout.fieldWidth, y + layout.rowHeight),
                value, AppCommand::None, 0, ProjectManagerUiState::FieldPayload(fieldId), 30);
    };

    int y = layout.top + 96;
    field(y, L"Width", std::to_wstring(uiState.formWidth), ProjectManagerActiveField::Width);
    y += layout.rowHeight + 10;
    field(y, L"Height", std::to_wstring(uiState.formHeight), ProjectManagerActiveField::Height);
    AddNode(scene, CoreUI::UiNodeType::Button,
            Rect(layout.fieldLeft + layout.fieldWidth + 12, y - layout.rowHeight - 10, layout.fieldLeft + layout.fieldWidth + 170, y + layout.rowHeight),
            L"Swap Orientation", AppCommand::SwapCanvasOrientation);
    y += layout.rowHeight + 10;
    field(y, L"Resolution", std::to_wstring(uiState.formDpi), ProjectManagerActiveField::Dpi);
    AddNode(scene, CoreUI::UiNodeType::Text, Rect(layout.fieldLeft + layout.fieldWidth + 12, y + 8, layout.fieldLeft + layout.fieldWidth + 64, y + layout.rowHeight),
            L"dpi", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    y += layout.rowHeight + 14;

    AddNode(scene, CoreUI::UiNodeType::Text, Rect(layout.contentLeft, y + 8, layout.contentLeft + layout.labelWidth, y + layout.rowHeight),
            L"Background", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.fieldLeft, y, layout.fieldLeft + layout.fieldWidth, y + layout.rowHeight),
            uiState.formTransparent ? L"Transparent" : L"White paper", AppCommand::ToggleCanvasTransparency);
    y += layout.rowHeight + 14;

    AddNode(scene, CoreUI::UiNodeType::Text, Rect(layout.contentLeft, y + 8, layout.contentLeft + layout.labelWidth, y + layout.rowHeight),
            L"Initial Layer", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.fieldLeft, y, layout.fieldLeft + layout.fieldWidth, y + layout.rowHeight),
            uiState.formTransparent ? L"Transparent Layer" : L"Color Layer", AppCommand::None, 0, L"", 30, Presentation::UiNodeVisible);
    y += layout.rowHeight + 18;

    AddNode(scene, CoreUI::UiNodeType::Text, Rect(layout.contentLeft, y, layout.contentLeft + 260, y + 24),
            L"RGB Profile", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    y += 28;
    const size_t profileLimit = std::min<size_t>(uiState.profiles.size(), 5);
    for (size_t i = 0; i < profileLimit; ++i) {
        const String label = (i == uiState.selectedRgbProfile ? L"* " : L"  ") +
                             (!uiState.profiles[i].displayName.empty() ? uiState.profiles[i].displayName : L"sRGB fallback");
        AddNode(scene, CoreUI::UiNodeType::Button,
                Rect(layout.fieldLeft, y + static_cast<int>(i) * 32, layout.contentRight - 12, y + static_cast<int>(i) * 32 + 28),
                label, AppCommand::SelectRgbProfile, i);
    }
    y += static_cast<int>(profileLimit) * 32 + 16;

    AddNode(scene, CoreUI::UiNodeType::Text, Rect(layout.contentLeft, y, layout.contentLeft + 260, y + 24),
            L"CMYK Profile", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    y += 28;
    for (size_t i = 0; i < profileLimit; ++i) {
        const String label = (i == uiState.selectedCmykProfile ? L"* " : L"  ") +
                             (!uiState.profiles[i].displayName.empty() ? uiState.profiles[i].displayName : L"sRGB fallback");
        AddNode(scene, CoreUI::UiNodeType::Button,
                Rect(layout.fieldLeft, y + static_cast<int>(i) * 32, layout.contentRight - 12, y + static_cast<int>(i) * 32 + 28),
                label, AppCommand::SelectCmykProfile, i);
    }

    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 350, layout.footerY, layout.contentRight - 236, layout.footerY + 38),
            L"Cancel", AppCommand::CloseModal);
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 224, layout.footerY, layout.contentRight, layout.footerY + 38),
            L"Create Image", AppCommand::CreateCanvasFromForm);
}

void BuildSettingsScene(const Size& viewport,
                        const ProjectManagerUiState&,
                        CoreUI::UiScene& scene) {
    const ProjectManagerLayout layout = ProjectManagerLayoutComputer::Compute(viewport, ProjectManagerPage::Settings);
    const wchar_t* categories[] = { L"General", L"Canvas", L"Color", L"Performance", L"Input", L"Files" };
    for (size_t i = 0; i < 6; ++i) {
        AddNode(scene, CoreUI::UiNodeType::ActionItem,
                Rect(16, 70 + static_cast<int>(i) * 46, layout.navWidth - 14, 110 + static_cast<int>(i) * 46),
                categories[i], AppCommand::SetSettingsCategory, i);
    }
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 36, 12, layout.contentRight - 8, 40),
            L"X", AppCommand::CloseModal, 0, L"", 60, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::X, CoreUI::IconPlacement::Center);

    const int footerY = layout.contentBottom - 52;
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 18 - 352, footerY, layout.contentRight - 18 - 246, footerY + 36),
            L"Apply", AppCommand::ApplySettings);
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 18 - 238, footerY, layout.contentRight - 18 - 132, footerY + 36),
            L"Cancel", AppCommand::CloseModal);
    AddNode(scene, CoreUI::UiNodeType::Button, Rect(layout.contentRight - 18 - 124, footerY, layout.contentRight - 18, footerY + 36),
            L"Save", AppCommand::SaveSettings);
}

} // namespace

void ProjectManagerComponentBuilder::Build(const Size& viewport,
                                           const ProjectManagerUiState& uiState,
                                           CoreUI::UiScene& scene) {
    scene.Clear();
    if (uiState.page == ProjectManagerPage::NewImage) {
        BuildNewImageScene(viewport, uiState, scene);
    } else if (uiState.page == ProjectManagerPage::Settings) {
        BuildSettingsScene(viewport, uiState, scene);
    }
}

} // namespace CloverPic::Core
