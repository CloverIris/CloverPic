#include "Core/UI/Workspace/Components/WorkspaceComponentBuilder.h"
#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/UI/Workspace/WorkspaceLayout.h"
#include "Core/Layers/BlendMode.h"
#include "Core/Presentation/IconPainter.h"
#include <algorithm>

namespace CloverPic::Core {

namespace {

using CoreUI::IconPlacement;
using CoreUI::UiBindingKey;
using CoreUI::UiInteractionKind;
using CoreUI::UiNodeType;

constexpr int MenuBarH = WorkspaceLayout::MenuBarH;
constexpr int TopBarH = WorkspaceLayout::TopBarH;
constexpr int StatusBarH = WorkspaceLayout::StatusBarH;
constexpr int ToolBarW = WorkspaceLayout::ToolBarW;

Presentation::UiNodeId AddNode(CoreUI::UiScene& scene,
                               UiNodeType type,
                               const Rect& bounds,
                               String label,
                               AppCommand command = AppCommand::None,
                               uint64_t userData = 0,
                               String payload = L"",
                               int z = 10,
                               uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                               Presentation::IconId iconId = Presentation::IconId::None,
                               IconPlacement iconPlacement = IconPlacement::None,
                               String tooltip = L"",
                               bool checked = false,
                               bool disabled = false,
                               String shortcut = L"",
                               bool hasSubmenu = false,
                               UiBindingKey bindingKey = UiBindingKey::None,
                               UiInteractionKind interactionKind = UiInteractionKind::Click,
                               WorkspacePanelId panelId = WorkspacePanelId::Color) {
    CoreUI::UiNode node;
    node.type = type;
    node.bounds = bounds;
    node.label = std::move(label);
    node.accessibilityLabel = node.label;
    node.tooltip = std::move(tooltip);
    node.iconId = iconId;
    node.iconPlacement = iconPlacement;
    node.command = static_cast<uint32_t>(command);
    node.userData = userData;
    node.payload = std::move(payload);
    node.shortcut = std::move(shortcut);
    node.zOrder = z;
    node.flags = flags;
    node.checked = checked;
    node.disabled = disabled;
    node.hasSubmenu = hasSubmenu;
    node.bindingKey = bindingKey;
    node.interactionKind = interactionKind;
    node.panelId = static_cast<uint16_t>(panelId);
    return scene.AddNode(std::move(node));
}

int ActiveMenuWidth(const String& key) {
    if (key == L"Filter") return 316;
    if (key == L"View") return 340;
    if (key == L"Tool") return 238;
    if (key == L"Window") return 248;
    if (key == L"Snap") return 256;
    return 282;
}

String BlendModeLabel(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return L"Normal";
        case BlendMode::Multiply: return L"Multiply";
        case BlendMode::Screen: return L"Screen";
        case BlendMode::Overlay: return L"Overlay";
        case BlendMode::Difference: return L"Difference";
        case BlendMode::Add: return L"Add";
        case BlendMode::Subtract: return L"Subtract";
        case BlendMode::Darken: return L"Darken";
        case BlendMode::Lighten: return L"Lighten";
        case BlendMode::ColorDodge: return L"Color Dodge";
        case BlendMode::ColorBurn: return L"Color Burn";
        case BlendMode::HardLight: return L"Hard Light";
        case BlendMode::SoftLight: return L"Soft Light";
        case BlendMode::Exclusion: return L"Exclusion";
        case BlendMode::Hue: return L"Hue";
        case BlendMode::Saturation: return L"Saturation";
        case BlendMode::Color: return L"Color";
        case BlendMode::Luminosity: return L"Luminosity";
    }
    return L"Normal";
}

constexpr BlendMode AllBlendModes[] = {
    BlendMode::Normal,
    BlendMode::Multiply,
    BlendMode::Screen,
    BlendMode::Overlay,
    BlendMode::Difference,
    BlendMode::Add,
    BlendMode::Subtract,
    BlendMode::Darken,
    BlendMode::Lighten,
    BlendMode::ColorDodge,
    BlendMode::ColorBurn,
    BlendMode::HardLight,
    BlendMode::SoftLight,
    BlendMode::Exclusion,
    BlendMode::Hue,
    BlendMode::Saturation,
    BlendMode::Color,
    BlendMode::Luminosity
};

void BuildWorkspaceChrome(const Size& viewport,
                          WorkspaceEditorFacade& editor,
                          const WorkspaceUiState& uiState,
                          const WorkspaceDockLayoutResult& layout,
                          CoreUI::UiScene& scene) {
    constexpr uint32_t visibleOnly = Presentation::UiNodeVisible;

    auto add = [&](Rect rect, String label, AppCommand command = AppCommand::None, uint64_t userData = 0,
                   UiNodeType type = UiNodeType::Button, int z = 20,
                   uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                   String payload = L"",
                   Presentation::IconId iconId = Presentation::IconId::None,
                   IconPlacement placement = IconPlacement::None,
                   String tooltip = L"",
                   bool checked = false,
                   bool disabled = false,
                   String shortcut = L"",
                   bool hasSubmenu = false,
                   UiBindingKey bindingKey = UiBindingKey::None,
                   UiInteractionKind interactionKind = UiInteractionKind::Click,
                   WorkspacePanelId panelId = WorkspacePanelId::Color) {
        AddNode(scene, type, rect, std::move(label), command, userData, std::move(payload), z, flags,
                iconId, placement, std::move(tooltip), checked, disabled, std::move(shortcut),
                hasSubmenu, bindingKey, interactionKind, panelId);
    };

    struct MenuDef {
        const wchar_t* key;
        const wchar_t* label;
        int width;
        bool enabled;
    };
    const MenuDef menus[] = {
        { L"File", L"File(F)", 52, true }, { L"Edit", L"Edit(E)", 54, true }, { L"Layer", L"Layer(L)", 66, true },
        { L"Filter", L"Filter(R)", 68, true }, { L"Select", L"Select(S)", 70, true }, { L"Snap", L"Snap(N)", 64, true },
        { L"Color", L"Color(C)", 70, true }, { L"View", L"View(V)", 62, true }, { L"Tool", L"Tool(T)", 58, true },
        { L"Window", L"Window(W)", 84, true }, { L"Help", L"Help", 58, false }
    };

    int menuX = 0;
    int activeMenuLeft = -1;
    int activeMenuWidth = 282;
    for (const auto& menu : menus) {
        const bool active = uiState.openMenu == menu.key;
        if (active) {
            activeMenuLeft = menuX;
            activeMenuWidth = ActiveMenuWidth(uiState.openMenu);
        }
        add(Rect(menuX, 0, menuX + menu.width, MenuBarH), menu.label, AppCommand::None, 0,
            UiNodeType::MenuHeader, 80,
            menu.enabled ? Presentation::UiNodeVisible | Presentation::UiNodeInteractive : Presentation::UiNodeVisible,
            menu.key);
        menuX += menu.width;
    }

    int iconX = 6;
    auto icon = [&](String label, AppCommand command, Presentation::IconId iconId, uint64_t data = 0, int width = 25,
                    uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive) {
        add(Rect(iconX, MenuBarH + 4, iconX + width, MenuBarH + 26), std::move(label), command, data,
            UiNodeType::Button, 30, flags, L"", iconId, IconPlacement::Center);
        iconX += width + 4;
    };
    icon(L"New", AppCommand::ShowNewCanvasModal, Presentation::IconId::FilePlus);
    icon(L"Open", AppCommand::OpenProject, Presentation::IconId::FolderOpen);
    icon(L"Save", AppCommand::SaveProject, Presentation::IconId::DeviceFloppy);
    icon(L"Save As", AppCommand::SaveProjectAs, Presentation::IconId::DeviceFloppyPlus, 0, 32);
    icon(L"Export", AppCommand::ExportPng, Presentation::IconId::FileExport, 0, 34);
    icon(L"Undo", AppCommand::Undo, Presentation::IconId::ArrowBackUp, 0, 28);
    icon(L"Redo", AppCommand::Redo, Presentation::IconId::ArrowForwardUp, 0, 28);
    icon(L"Fit", AppCommand::FitCanvas, Presentation::IconId::ArrowsMaximize, 0, 32);
    icon(L"1:1", AppCommand::SetZoomPreset, Presentation::IconId::ZoomReset, 100, 32);
    add(Rect(iconX + 6, MenuBarH + 8, iconX + 190, MenuBarH + 24), L"AntiAliasing | Correction 0 | Soft Edge",
        AppCommand::None, 0, UiNodeType::Text, 5, visibleOnly);
    add(Rect(viewport.width - 160, MenuBarH + 4, viewport.width - 8, MenuBarH + 26), L"Program Manager", AppCommand::GoHome, 0,
        UiNodeType::Button, 32, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
        Presentation::IconId::Home, IconPlacement::Leading);

    const ToolType tools[] = {
        ToolType::Brush, ToolType::Eraser, ToolType::Move, ToolType::Eyedropper,
        ToolType::Fill, ToolType::RectSelect, ToolType::Shape
    };
    const wchar_t* toolLabels[] = { L"Brush", L"Erase", L"Move", L"Pick", L"Fill", L"Select", L"Shape" };
    const Presentation::IconId toolIcons[] = {
        Presentation::IconId::Brush, Presentation::IconId::Eraser, Presentation::IconId::Move,
        Presentation::IconId::ColorPicker, Presentation::IconId::Bucket, Presentation::IconId::Select,
        Presentation::IconId::Shape
    };
    int toolY = TopBarH + 4;
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); ++i) {
        add(Rect(6, toolY, ToolBarW - 6, toolY + 54), toolLabels[i], AppCommand::SelectTool,
            static_cast<uint64_t>(tools[i]), UiNodeType::ToolbarItem, 25,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"", toolIcons[i], IconPlacement::Center);
        toolY += 60;
    }

    add(layout.leftToggleRect, uiState.leftSidebarExpanded ? L"<" : L">", AppCommand::ToggleLeftSidebar, 0,
        UiNodeType::Button, 36);
    add(layout.rightToggleRect, uiState.rightSidebarExpanded ? L">" : L"<", AppCommand::ToggleRightSidebar, 0,
        UiNodeType::Button, 36);

    auto addMenuItem = [&](int index, String label, AppCommand command = AppCommand::None, uint64_t data = 0,
                           bool enabled = true, String payload = L"", bool checked = false, bool hasSubmenu = false) {
        if (activeMenuLeft < 0) return;
        const int itemTop = MenuBarH + 8 + index * 28;
        String shortcut;
        const size_t sep = label.find(L'|');
        if (sep != String::npos) {
            shortcut = label.substr(sep + 1);
            label = label.substr(0, sep);
        }
        const AppCommand finalCommand = enabled ? command : AppCommand::ShowUnavailable;
        add(Rect(activeMenuLeft, itemTop, activeMenuLeft + activeMenuWidth, itemTop + 26),
            std::move(label), finalCommand, data, UiNodeType::MenuItem, 120,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            std::move(payload), Presentation::IconId::None, IconPlacement::None, L"",
            checked, !enabled, std::move(shortcut), hasSubmenu);
    };
    auto addMenuSeparator = [&](int index) {
        if (activeMenuLeft < 0) return;
        const int itemTop = MenuBarH + 8 + index * 28;
        add(Rect(activeMenuLeft + 8, itemTop + 12, activeMenuLeft + activeMenuWidth - 8, itemTop + 13),
            L"", AppCommand::None, 0, UiNodeType::MenuSeparator, 119, visibleOnly);
    };

    if (uiState.openMenu == L"File") {
        addMenuItem(0, L"New(N)...|Ctrl+N", AppCommand::ShowNewCanvasModal);
        addMenuItem(1, L"Open(O)...|Ctrl+O", AppCommand::OpenProject);
        addMenuItem(2, L"Open Recent File(R)|", AppCommand::ShowUnavailable, 0, false, L"", false, true);
        addMenuSeparator(3);
        addMenuItem(4, L"Save(S)|Ctrl+S", AppCommand::SaveProject, 0, editor.HasProject());
        addMenuItem(5, L"Save As(A)...|Ctrl+Shift+S", AppCommand::SaveProjectAs, 0, editor.HasProject());
        addMenuItem(6, L"Export...|", AppCommand::ExportPng, 0, editor.HasProject());
        addMenuSeparator(7);
        addMenuItem(8, L"Environment Settings(K)...|Ctrl+K", AppCommand::ShowSettingsModal);
        addMenuItem(9, L"Close Project|Alt+F4", AppCommand::CloseWorkspace);
    } else if (uiState.openMenu == L"Edit") {
        addMenuItem(0, L"Undo(Z)|Ctrl+Z", AppCommand::Undo, 0, editor.HasProject());
        addMenuItem(1, L"Redo(Y)|Ctrl+Y", AppCommand::Redo, 0, editor.HasProject());
        addMenuSeparator(2);
        addMenuItem(3, L"Cut(T)|Ctrl+X", AppCommand::None, 0, false);
        addMenuItem(4, L"Copy(C)|Ctrl+C", AppCommand::None, 0, false);
        addMenuItem(5, L"Paste(V)|Ctrl+V", AppCommand::None, 0, false);
        addMenuSeparator(6);
        addMenuItem(7, L"Canvas Size(S)...|Ctrl+Alt+C", AppCommand::ShowCanvasSizeModal, 0, editor.HasProject());
        addMenuItem(8, L"Rotate Left(L)|Left", AppCommand::RotateLayerLeft, 0, editor.HasProject());
        addMenuItem(9, L"Rotate Right(R)|Right", AppCommand::RotateLayerRight, 0, editor.HasProject());
        addMenuItem(10, L"Flip Horizontally(H)|Down", AppCommand::FlipLayerHorizontal, 0, editor.HasProject());
        addMenuItem(11, L"Flip Vertically(V)|", AppCommand::FlipLayerVertical, 0, editor.HasProject());
    } else if (uiState.openMenu == L"Filter") {
        addMenuItem(0, L"Levels(L)...|Ctrl+L", AppCommand::ShowUnavailable, 0, false);
        addMenuItem(1, L"Hue(H)...|Ctrl+U", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::HueSaturation, 60), editor.HasProject());
        addMenuItem(2, L"Brightness / Contrast(B)...|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::BrightnessContrast, 56), editor.HasProject());
        addMenuSeparator(3);
        addMenuItem(4, L"Reverse Color|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Invert), editor.HasProject());
        addMenuItem(5, L"Threshold|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Threshold, 128), editor.HasProject());
        addMenuItem(6, L"Gaussian Blur(G)...|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::GaussianBlur, 2), editor.HasProject());
        addMenuItem(7, L"Unsharp Mask...|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Sharpen, 40), editor.HasProject());
        addMenuSeparator(8);
        addMenuItem(9, L"Channel Operation (O)...|", AppCommand::None, 0, false);
        addMenuItem(10, L"Tone Curve (T)...|", AppCommand::None, 0, false);
    } else if (uiState.openMenu == L"Select") {
        addMenuItem(0, L"Select All(A)|Ctrl+A", AppCommand::SelectAll, 0, editor.HasProject());
        addMenuItem(1, L"Clear(D)|Ctrl+D", AppCommand::ClearSelection, 0, editor.HasProject());
        addMenuItem(2, L"Invert(I)|Ctrl+Shift+I", AppCommand::InvertSelection, 0, editor.HasProject());
        addMenuItem(3, L"Rectangle Select(R)|M", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::RectSelect), editor.HasProject());
        addMenuItem(4, L"Lasso(L)|", AppCommand::None, 0, false);
        addMenuItem(5, L"MagicWand(W)|", AppCommand::None, 0, false);
    } else if (uiState.openMenu == L"Snap") {
        addMenuItem(0, L"Off(O)|1", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Off), true, L"", editor.GetSnapMode() == SnapModeId::Off);
        addMenuItem(1, L"Parallel(P)|2", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Parallel), true, L"", editor.GetSnapMode() == SnapModeId::Parallel);
        addMenuItem(2, L"Crisscross(C)|3", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Crisscross), true, L"", editor.GetSnapMode() == SnapModeId::Crisscross);
        addMenuItem(3, L"Vanishing Point(V)|4", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::VanishingPoint), true, L"", editor.GetSnapMode() == SnapModeId::VanishingPoint);
        addMenuItem(4, L"Radial(R)|5", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Radial), true, L"", editor.GetSnapMode() == SnapModeId::Radial);
        addMenuItem(5, L"Circle(E)|6", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Circle), true, L"", editor.GetSnapMode() == SnapModeId::Circle);
        addMenuItem(6, L"Curve(K)|7", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Curve), true, L"", editor.GetSnapMode() == SnapModeId::Curve);
        addMenuItem(7, L"Curved line (ellipse)(L)|8", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::CurvedLineEllipse), true, L"", editor.GetSnapMode() == SnapModeId::CurvedLineEllipse);
    } else if (uiState.openMenu == L"Color") {
        addMenuItem(0, L"Transparent(A)|Shift+Z", AppCommand::ToggleViewOption, static_cast<uint64_t>(ViewOptionId::TransparentBackground), true,
                    L"", editor.IsViewOptionEnabled(ViewOptionId::TransparentBackground));
        addMenuItem(1, L"Swap Fore/BG(S)|X", AppCommand::ShowUnavailable, 0, false);
        addMenuItem(2, L"Initialize(I)|D", AppCommand::ShowUnavailable, 0, false);
        addMenuSeparator(3);
        addMenuItem(4, L"Color Bar(B)|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Color), true,
                    L"", uiState.IsPanelVisible(WorkspacePanelId::Color));
        addMenuItem(5, L"Color Wheel(W)|", AppCommand::None, 0, false);
        addMenuItem(6, L"Color Wheel Triangle|", AppCommand::None, 0, false);
        addMenuItem(7, L"Lock Palette|", AppCommand::None, 0, false);
    } else if (uiState.openMenu == L"View") {
        addMenuItem(0, L"Zoom In(I)|Ctrl++", AppCommand::ZoomIn, 0, editor.HasProject());
        addMenuItem(1, L"Zoom Out(O)|Ctrl+-", AppCommand::ZoomOut, 0, editor.HasProject());
        addMenuItem(2, L"Fit to Window Size(F)|Ctrl+0", AppCommand::FitCanvas, 0, editor.HasProject());
        addMenuItem(3, L"100%|", AppCommand::SetZoomPreset, 100, editor.HasProject());
        addMenuItem(4, L"200%|", AppCommand::SetZoomPreset, 200, editor.HasProject());
        addMenuSeparator(5);
        addMenuItem(6, L"Grid(G)|Ctrl+G", AppCommand::ToggleViewOption, static_cast<uint64_t>(ViewOptionId::Grid), true,
                    L"", editor.IsViewOptionEnabled(ViewOptionId::Grid));
        addMenuItem(7, L"Pixel Grid(P)|", AppCommand::ToggleViewOption, static_cast<uint64_t>(ViewOptionId::PixelGrid), true,
                    L"", editor.IsViewOptionEnabled(ViewOptionId::PixelGrid));
        addMenuItem(8, L"Transparent Background(T)|", AppCommand::ToggleViewOption, static_cast<uint64_t>(ViewOptionId::TransparentBackground), true,
                    L"", editor.IsViewOptionEnabled(ViewOptionId::TransparentBackground));
        addMenuItem(9, L"Use and display the color profile.|", AppCommand::ToggleViewOption, static_cast<uint64_t>(ViewOptionId::ColorProfileDisplay), true,
                    L"", editor.IsViewOptionEnabled(ViewOptionId::ColorProfileDisplay));
        addMenuItem(10, L"Color Management Settings...|", AppCommand::ShowSettingsModal);
    } else if (uiState.openMenu == L"Tool") {
        addMenuItem(0, L"Brush|B", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Brush), true, L"", editor.GetTool() == ToolType::Brush);
        addMenuItem(1, L"Eraser|E", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Eraser), true, L"", editor.GetTool() == ToolType::Eraser);
        addMenuItem(2, L"Move|V", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Move), true, L"", editor.GetTool() == ToolType::Move);
        addMenuItem(3, L"Fill|G", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Fill), true, L"", editor.GetTool() == ToolType::Fill);
        addMenuItem(4, L"Select|M", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::RectSelect), true, L"", editor.GetTool() == ToolType::RectSelect);
        addMenuItem(5, L"Eyedropper|I", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Eyedropper), true, L"", editor.GetTool() == ToolType::Eyedropper);
        addMenuItem(6, L"Shape|U", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Shape), true, L"", editor.GetTool() == ToolType::Shape);
        addMenuItem(7, L"Text|T", AppCommand::None, 0, false);
    } else if (uiState.openMenu == L"Window") {
        addMenuItem(0, L"Initialize(I)...|", AppCommand::InitializeLayout);
        addMenuItem(1, L"Show/Hide(S)|Tab", AppCommand::ShowUnavailable, 0, false);
        addMenuSeparator(2);
        addMenuItem(3, L"Color|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Color), true, L"", uiState.IsPanelVisible(WorkspacePanelId::Color));
        addMenuItem(4, L"Brush Preview|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushPreview), true, L"", uiState.IsPanelVisible(WorkspacePanelId::BrushPreview));
        addMenuItem(5, L"Brush Control|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushControl), true, L"", uiState.IsPanelVisible(WorkspacePanelId::BrushControl));
        addMenuItem(6, L"Brush|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushPresets), true, L"", uiState.IsPanelVisible(WorkspacePanelId::BrushPresets));
        addMenuItem(7, L"Navigator|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Navigator), true, L"", uiState.IsPanelVisible(WorkspacePanelId::Navigator));
        addMenuItem(8, L"Layer|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Layer), true, L"", uiState.IsPanelVisible(WorkspacePanelId::Layer));
        addMenuItem(9, L"Brush Size|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushSize), true, L"", uiState.IsPanelVisible(WorkspacePanelId::BrushSize));
        addMenuItem(10, L"Status Bar|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::StatusBar), true, L"", uiState.IsPanelVisible(WorkspacePanelId::StatusBar));
    }
}

void BuildPanelChrome(const WorkspacePanelComputedLayout& panel,
                      CoreUI::UiScene& scene) {
    AddNode(scene, UiNodeType::Panel, panel.rect, PanelTitle(panel.panelId),
            AppCommand::None, 0, L"", panel.zOrder,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::PanelHeader, panel.headerRect, PanelTitle(panel.panelId),
            AppCommand::None, 0, L"", panel.zOrder + 1,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::DragPanel, panel.panelId);

    AddNode(scene, UiNodeType::Button,
            Rect(panel.headerRect.right - 18, panel.headerRect.top + 4, panel.headerRect.right - 4, panel.headerRect.bottom - 4),
            L"x", AppCommand::TogglePanel, static_cast<uint64_t>(panel.panelId), L"", panel.zOrder + 2,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::Center, L"Close panel",
            false, false, L"", false, UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
}

void BuildColorPanel(const WorkspacePanelComputedLayout& panel, CoreUI::UiScene& scene) {
    const int colorFieldSize = std::max(120, std::min(216, panel.contentRect.Width() - 34));
    const int fieldLeft = panel.contentRect.left + 36;
    const int fieldTop = panel.contentRect.top;
    AddNode(scene, UiNodeType::ColorField, Rect(fieldLeft, fieldTop, fieldLeft + colorFieldSize, fieldTop + colorFieldSize),
            L"Color field", AppCommand::None, 0, L"", panel.zOrder + 4,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            UiBindingKey::ColorField, UiInteractionKind::DragValue, panel.panelId);
    AddNode(scene, UiNodeType::HueStrip,
            Rect(fieldLeft + colorFieldSize + 6, fieldTop, fieldLeft + colorFieldSize + 24, fieldTop + colorFieldSize),
            L"Hue", AppCommand::None, 0, L"", panel.zOrder + 4,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            UiBindingKey::HueStrip, UiInteractionKind::DragValue, panel.panelId);

    const uint64_t swatches[] = {
        SwatchBlack, SwatchWhite, SwatchRed, SwatchBlue, SwatchGreen, SwatchYellow,
        SwatchPurple, 0x00A4E4FFull, 0xFF8C00FFull, 0x107C10FFull, 0x5C2D91FFull, SwatchTransparent
    };
    const int swatchTop = fieldTop + colorFieldSize + 12;
    for (int i = 0; i < 12; ++i) {
        const int sx = panel.contentRect.left + (i % 6) * 34;
        const int sy = swatchTop + (i / 6) * 28;
        AddNode(scene, UiNodeType::Swatch, Rect(sx, sy, sx + 26, sy + 20), L"", AppCommand::SetColor, swatches[i], L"",
                panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
                UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    }
}

void AddSliderNode(CoreUI::UiScene& scene,
                   const WorkspacePanelComputedLayout& panel,
                   const Rect& rect,
                   const String& label,
                   AppCommand command,
                   uint64_t userData,
                   UiBindingKey bindingKey) {
    AddNode(scene, UiNodeType::Slider, rect, label, command, userData, L"", panel.zOrder + 4,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            bindingKey, UiInteractionKind::DragValue, panel.panelId);
}

void BuildBrushControlPanel(const WorkspacePanelComputedLayout& panel, CoreUI::UiScene& scene, WorkspaceEditorFacade& editor) {
    const int left = panel.contentRect.left;
    const int right = panel.contentRect.right;
    AddSliderNode(scene, panel, Rect(left, panel.contentRect.top + 2, right, panel.contentRect.top + 26), L"Size",
                  AppCommand::SetBrushParam, PackBrushParam(BrushParamId::Size, static_cast<uint16_t>(std::clamp<int>(static_cast<int>(editor.GetBrushSize()), 1, 100))),
                  UiBindingKey::BrushSize);
    AddSliderNode(scene, panel, Rect(left, panel.contentRect.top + 30, right, panel.contentRect.top + 54), L"Opacity",
                  AppCommand::SetBrushParam, PackBrushParam(BrushParamId::Opacity, 0), UiBindingKey::BrushOpacity);
    AddSliderNode(scene, panel, Rect(left, panel.contentRect.top + 58, right, panel.contentRect.top + 82), L"Flow",
                  AppCommand::SetBrushParam, PackBrushParam(BrushParamId::Flow, 0), UiBindingKey::BrushFlow);
    AddSliderNode(scene, panel, Rect(left, panel.contentRect.top + 86, right, panel.contentRect.top + 110), L"Spacing",
                  AppCommand::SetBrushParam, PackBrushParam(BrushParamId::Spacing, 0), UiBindingKey::BrushSpacing);
    AddSliderNode(scene, panel, Rect(left, panel.contentRect.top + 114, right, panel.contentRect.top + 138), L"Wet Mix",
                  AppCommand::SetBrushParam, PackBrushParam(BrushParamId::WetMix, 0), UiBindingKey::BrushWetMix);

    const int tipTop = panel.contentRect.top + 150;
    AddNode(scene, UiNodeType::Button, Rect(left, tipTop, left + 72, tipTop + 24), L"Hard", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::RoundHard), L"", panel.zOrder + 4);
    AddNode(scene, UiNodeType::Button, Rect(left + 78, tipTop, left + 150, tipTop + 24), L"Soft", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::RoundSoft), L"", panel.zOrder + 4);
    AddNode(scene, UiNodeType::Button, Rect(left + 156, tipTop, left + 228, tipTop + 24), L"Flat", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::Flat), L"", panel.zOrder + 4);
    AddNode(scene, UiNodeType::Button, Rect(left, tipTop + 30, left + 104, tipTop + 54), L"Bristle", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::Bristle), L"", panel.zOrder + 4);
    AddNode(scene, UiNodeType::Button, Rect(left + 110, tipTop + 30, left + 214, tipTop + 54), L"Texture", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::Texture), L"", panel.zOrder + 4);
}

void BuildBrushPresetPanel(const WorkspacePanelComputedLayout& panel, CoreUI::UiScene& scene) {
    struct PresetDef {
        const wchar_t* label;
        uint16_t size;
        Render::BrushTipType tip;
        uint64_t accent;
    };
    const PresetDef presets[] = {
        { L"15 Pen", 15, Render::BrushTipType::RoundHard, 0x3B82F6FFull },
        { L"8 Pen Soft", 8, Render::BrushTipType::RoundSoft, 0x7DD3FCFFull },
        { L"20 Mapping Pen", 20, Render::BrushTipType::Flat, 0xEF4444FFull },
        { L"30 Kabura Pen", 30, Render::BrushTipType::Bristle, 0xF97316FFull },
        { L"40 Watercolor", 40, Render::BrushTipType::Texture, 0x34D399FFull },
        { L"60 Texture Pen", 60, Render::BrushTipType::Texture, 0xA78BFAFFull }
    };
    int y = panel.contentRect.top;
    for (const auto& preset : presets) {
        if (y + 34 > panel.contentRect.bottom) {
            break;
        }
        AddNode(scene, UiNodeType::BrushPresetItem, Rect(panel.contentRect.left, y, panel.contentRect.right, y + 30),
                preset.label, AppCommand::SetBrushPreset, PackBrushPreset(preset.size, static_cast<uint16_t>(preset.tip)), L"",
                panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
                UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
        y += 34;
    }
}

void BuildNavigatorPanel(const WorkspacePanelComputedLayout& panel, CoreUI::UiScene& scene) {
    const int y = panel.contentRect.bottom - 30;
    AddNode(scene, UiNodeType::Button, Rect(panel.contentRect.left, y, panel.contentRect.left + 50, y + 24), L"50%",
            AppCommand::SetZoomPreset, 50, L"", panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::ZoomIn, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, Rect(panel.contentRect.left + 56, y, panel.contentRect.left + 108, y + 24), L"100%",
            AppCommand::SetZoomPreset, 100, L"", panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::ZoomIn, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, Rect(panel.contentRect.left + 114, y, panel.contentRect.left + 166, y + 24), L"200%",
            AppCommand::SetZoomPreset, 200, L"", panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::ZoomIn, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, Rect(panel.contentRect.left + 172, y, panel.contentRect.right, y + 24), L"Reset",
            AppCommand::ResetView, 0, L"", panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::ArrowsMaximize, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
}

void BuildLayerPanel(const WorkspacePanelComputedLayout& panel,
                     CoreUI::UiScene& scene,
                     WorkspaceEditorFacade& editor,
                     const WorkspaceUiState& uiState) {
    auto* lm = editor.GetLayerManager();
    uint8_t activeOpacity = 255;
    BlendMode activeBlend = BlendMode::Normal;
    bool protectAlpha = false;
    bool locked = false;
    bool visible = false;
    bool solo = false;
    if (lm) {
        if (auto active = lm->GetActiveLayer()) {
            activeOpacity = active->GetOpacity();
            activeBlend = active->GetBlendMode();
            protectAlpha = active->IsProtectAlpha();
            locked = active->IsLocked();
            visible = active->IsVisible();
            solo = lm->IsSoloActive() && lm->GetSoloLayerIndex() == lm->GetActiveLayerIndex();
        }
    }
    AddSliderNode(scene, panel, Rect(panel.contentRect.left, panel.contentRect.top + 2, panel.contentRect.right, panel.contentRect.top + 26), L"Opacity",
                  AppCommand::SetLayerOpacity, activeOpacity, UiBindingKey::LayerOpacity);

    const Rect blendRect(panel.contentRect.left, panel.contentRect.top + 34, panel.contentRect.right, panel.contentRect.top + 60);
    AddNode(scene, UiNodeType::ComboBox, blendRect, BlendModeLabel(activeBlend), AppCommand::ToggleLayerBlendDropdown, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::Layers, IconPlacement::Leading, L"Blend mode", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    if (uiState.layerBlendDropdownOpen) {
        const int itemH = 24;
        const int dropdownH = static_cast<int>(sizeof(AllBlendModes) / sizeof(AllBlendModes[0])) * itemH + 4;
        const Rect dropdown(panel.contentRect.left, blendRect.bottom + 3, panel.contentRect.right,
                            blendRect.bottom + 3 + dropdownH);
        AddNode(scene, UiNodeType::Panel, dropdown, L"", AppCommand::None, 0, L"", panel.zOrder + 90,
                Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
                UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
        int itemY = dropdown.top + 2;
        for (BlendMode mode : AllBlendModes) {
            AddNode(scene, UiNodeType::MenuItem, Rect(dropdown.left + 2, itemY, dropdown.right - 2, itemY + itemH),
                    BlendModeLabel(mode), AppCommand::SetBlendMode, static_cast<uint64_t>(mode), L"",
                    panel.zOrder + 92, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                    Presentation::IconId::None, IconPlacement::None, L"", mode == activeBlend, false, L"", false,
                    UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
            itemY += itemH;
        }
    }

    const int toggleTop = panel.contentRect.top + 70;
    const int gap = 6;
    const int toggleW = std::max(44, (panel.contentRect.Width() - gap * 3) / 4);
    auto addToggle = [&](int column, const String& label, AppCommand command, Presentation::IconId iconId, bool checked, bool disabled = false) {
        const int left = panel.contentRect.left + column * (toggleW + gap);
        AddNode(scene, UiNodeType::Button, Rect(left, toggleTop, left + toggleW, toggleTop + 26), label, command, 0, L"",
                panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                iconId, IconPlacement::Leading, L"", checked, disabled, L"", false,
                UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    };
    addToggle(0, L"Vis", AppCommand::ToggleLayerVisibility, visible ? Presentation::IconId::Eye : Presentation::IconId::EyeOff, visible);
    addToggle(1, L"Alpha", AppCommand::ToggleProtectAlpha, Presentation::IconId::Layers, protectAlpha);
    addToggle(2, L"Lock", AppCommand::ToggleLayerLock, Presentation::IconId::Lock, locked);
    addToggle(3, L"Solo", AppCommand::ToggleSolo, Presentation::IconId::Eye, solo);

    const int listTop = panel.contentRect.top + 106;
    const int layerToolsY = panel.contentRect.bottom - 66;
    if (lm) {
        int y = listTop;
        for (size_t i = 0; i < lm->GetLayerCount() && y + 54 < layerToolsY - 8; ++i) {
            auto layer = lm->GetLayer(i);
            String label = layer ? layer->GetName() : L"Layer";
            AddNode(scene, UiNodeType::LayerItem, Rect(panel.contentRect.left, y, panel.contentRect.right, y + 52), label,
                    AppCommand::SelectLayer, static_cast<uint64_t>(i), L"", panel.zOrder + 5,
                    Presentation::UiNodeVisible | Presentation::UiNodeInteractive, Presentation::IconId::None, IconPlacement::None,
                    L"", false, false, L"", false, UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
            y += 56;
        }
    }

    const int row1 = panel.contentRect.bottom - 60;
    const int row2 = panel.contentRect.bottom - 28;
    const int actionGap = 6;
    const int actionW = std::max(42, (panel.contentRect.Width() - actionGap * 3) / 4);
    auto actionRect = [&](int rowTop, int column, int columns = 1) {
        const int left = panel.contentRect.left + column * (actionW + actionGap);
        return Rect(left, rowTop, left + actionW * columns + actionGap * (columns - 1), rowTop + 25);
    };
    AddNode(scene, UiNodeType::Button, actionRect(row1, 0), L"Add", AppCommand::AddLayer, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::Plus, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row1, 1), L"Text", AppCommand::ShowUnavailable, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::TextPlus, IconPlacement::Leading, L"", false, true, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row1, 2), L"Dup", AppCommand::DuplicateLayer, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::Copy, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row1, 3), L"Merge", AppCommand::MergeLayerDown, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::Merge, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row2, 0), L"Solo", AppCommand::ToggleSolo, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::Eye, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row2, 1), L"Del", AppCommand::DeleteLayer, 0, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::Trash, IconPlacement::Leading, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row2, 2), L"50%", AppCommand::SetLayerOpacity, 128, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    AddNode(scene, UiNodeType::Button, actionRect(row2, 3), L"100%", AppCommand::SetLayerOpacity, 255, L"",
            panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
            UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
}

void BuildBrushSizePanel(const WorkspacePanelComputedLayout& panel, CoreUI::UiScene& scene) {
    const uint16_t sizes[] = { 1, 3, 5, 7, 10, 12, 15, 20, 25, 30, 40, 50, 70, 100 };
    for (int i = 0; i < 14; ++i) {
        const int cx = panel.contentRect.left + 2 + (i % 7) * 41;
        const int cy = panel.contentRect.top + 4 + (i / 7) * 50;
        AddNode(scene, UiNodeType::BrushSizeChip, Rect(cx, cy, cx + 34, cy + 42), std::to_wstring(sizes[i]), AppCommand::SetBrushParam,
                PackBrushParam(BrushParamId::Size, sizes[i]), L"", panel.zOrder + 4, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                Presentation::IconId::None, IconPlacement::None, L"", false, false, L"", false,
                UiBindingKey::None, UiInteractionKind::Click, panel.panelId);
    }
}

} // namespace

void WorkspaceComponentBuilder::Build(const Size& viewport,
                                      WorkspaceEditorFacade& editor,
                                      const WorkspaceUiState& uiState,
                                      const WorkspaceDockLayoutResult& layout,
                                      CoreUI::UiScene& scene) {
    scene.Clear();
    BuildWorkspaceChrome(viewport, editor, uiState, layout, scene);

    for (const auto& panel : layout.panels) {
        BuildPanelChrome(panel, scene);
        switch (panel.panelId) {
            case WorkspacePanelId::Color:
                BuildColorPanel(panel, scene);
                break;
            case WorkspacePanelId::BrushControl:
                BuildBrushControlPanel(panel, scene, editor);
                break;
            case WorkspacePanelId::BrushPresets:
                BuildBrushPresetPanel(panel, scene);
                break;
            case WorkspacePanelId::Navigator:
                BuildNavigatorPanel(panel, scene);
                break;
            case WorkspacePanelId::Layer:
                BuildLayerPanel(panel, scene, editor, uiState);
                break;
            case WorkspacePanelId::BrushSize:
                BuildBrushSizePanel(panel, scene);
                break;
            case WorkspacePanelId::BrushPreview:
            case WorkspacePanelId::StatusBar:
            default:
                break;
        }
    }
}

} // namespace CloverPic::Core
