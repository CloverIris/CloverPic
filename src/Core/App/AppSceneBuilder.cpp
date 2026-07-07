#include "Core/App/AppSceneBuilder.h"
#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"
#include <algorithm>
#include <cmath>
#include <cwctype>

namespace CloverPic::Core {

namespace {

constexpr int MenuBarH = 24;
constexpr int CommandBarH = 30;
constexpr int TopBarH = MenuBarH + CommandBarH;
constexpr int StatusBarH = 24;
constexpr int ToolBarW = 64;
constexpr int LeftPanelW = 292;
constexpr int RightPanelW = 312;

Rect FullRect(const RgbaFrame& frame) {
    return Rect(0, 0, static_cast<int32_t>(frame.width), static_cast<int32_t>(frame.height));
}

String CompactPathLabel(const String& path, size_t maxChars = 34) {
    if (path.size() <= maxChars) return path;
    const size_t tail = maxChars > 4 ? maxChars - 4 : maxChars;
    return L"..." + path.substr(path.size() - tail);
}

String ToLower(String value) {
    for (auto& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

bool MatchesQuery(const String& value, const String& query) {
    if (query.empty()) return true;
    return ToLower(value).find(ToLower(query)) != String::npos;
}

String FileNameFromPath(const String& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == String::npos ? path : path.substr(slash + 1);
}

Presentation::UiNodeId AddNode(CoreUI::UiScene& scene,
                               CoreUI::UiNodeType type,
                               const Rect& bounds,
                               String label,
                               AppCommand command = AppCommand::None,
                               uint64_t userData = 0,
                               String payload = L"",
                               int z = 10,
                               uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                               Presentation::IconId iconId = Presentation::IconId::None,
                               CoreUI::IconPlacement iconPlacement = CoreUI::IconPlacement::None,
                               String tooltip = L"",
                               bool checked = false,
                               bool disabled = false,
                               String shortcut = L"",
                               bool hasSubmenu = false) {
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
    return scene.AddNode(std::move(node));
}

} // namespace

void AppSceneBuilder::Build(AppScreen screen,
                            ModalKind modal,
                            const Size& viewport,
                            const RgbaFrame& frame,
                            const EditorSession& session,
                            CanvasController& canvas,
                            const String& homeSearchQuery,
                            CoreUI::UiScene& scene,
                            const String& workspaceOpenMenu) {
    scene.Clear();
    if (screen == AppScreen::Home) {
        BuildHomeScene(viewport, session, homeSearchQuery, scene);
    } else {
        BuildWorkspaceScene(viewport, session, canvas, scene, workspaceOpenMenu);
    }
    BuildModalScene(modal, viewport, frame, scene);
}

void AppSceneBuilder::BuildHomeScene(const Size& viewport,
                                     const EditorSession& session,
                                     const String& searchQuery,
                                     CoreUI::UiScene& scene) {
    const int margin = std::max(24, viewport.width / 36);
    const int leftW = std::clamp(viewport.width / 4, 260, 360);
    const int searchH = 44;
    const int actionH = 42;
    const int tileGap = 8;
    const int left = margin;
    const int top = margin + 18;
    const int bottom = viewport.height - margin;
    const int tileLeft = left + leftW + std::max(32, viewport.width / 24);
    const int tileRight = viewport.width - margin;
    const int tileTop = top + 42;

    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(left, top + 86, left + leftW, top + 86 + actionH),
            L"NEW CANVAS", AppCommand::ShowNewCanvasModal, 0, L"", 12);
    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(left, top + 136, left + leftW, top + 136 + actionH),
            L"OPEN PROJECT", AppCommand::OpenProject, 0, L"", 12);
    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(left, bottom - 108, left + leftW, bottom - 108 + actionH),
            L"SETTINGS", AppCommand::ShowSettingsModal, 0, L"", 8);
    AddNode(scene, CoreUI::UiNodeType::ActionItem, Rect(left, bottom - 58, left + leftW, bottom - 58 + actionH),
            L"EXIT", AppCommand::Quit, 0, L"", 8);
    AddNode(scene, CoreUI::UiNodeType::SearchBox, Rect(left, bottom - searchH - 128, left + leftW, bottom - 128),
            searchQuery.empty() ? L"SEARCH RECENT PROJECTS" : searchQuery, AppCommand::FocusHomeSearch, 0, L"", 20);

    const auto recent = session.GetRecentFiles();
    int recentY = top + 226;
    int recentCount = 0;
    for (const auto& path : recent) {
        if (path.empty() || !MatchesQuery(path, searchQuery)) continue;
        if (recentY + 42 > bottom - searchH - 146) break;
        const String label = FileNameFromPath(path).empty() ? CompactPathLabel(path) : FileNameFromPath(path);
        AddNode(scene, CoreUI::UiNodeType::RecentItem, Rect(left, recentY, left + leftW, recentY + 42),
                CompactPathLabel(label, 28), AppCommand::OpenRecentProject, 0, path, 15);
        recentY += 48;
        ++recentCount;
    }

    if (recentCount == 0) {
        const String emptyLabel = searchQuery.empty() ? L"NO RECENT PROJECTS YET" : L"NO MATCHING PROJECTS";
        AddNode(scene, CoreUI::UiNodeType::Text, Rect(left, recentY, left + leftW, recentY + 28),
                emptyLabel, AppCommand::None, 0, L"", 1, Presentation::UiNodeVisible);
    }

    const int unit = std::clamp((tileRight - tileLeft - tileGap * 5) / 6, 74, 126);
    const int sectionGap = std::max(24, unit / 4);
    const int group1 = tileLeft;
    const int group2 = group1 + unit * 3 + tileGap * 3 + sectionGap;
    const int row = tileTop;
    const uint32_t visibleOnly = Presentation::UiNodeVisible;
    auto tile = [&](int x, int y, int wUnits, int hUnits, const String& label, uint64_t color,
                    AppCommand command = AppCommand::None) {
        AddNode(scene,
                CoreUI::UiNodeType::Tile,
                Rect(x, y, x + unit * wUnits + tileGap * (wUnits - 1), y + unit * hUnits + tileGap * (hUnits - 1)),
                label,
                command,
                color,
                L"",
                6,
                command == AppCommand::None ? visibleOnly : Presentation::UiNodeVisible | Presentation::UiNodeInteractive);
    };

    tile(group1, row, 1, 1, L"NEW", 0x0078D7FFull, AppCommand::ShowNewCanvasModal);
    tile(group1 + unit + tileGap, row, 2, 1, L"CANVAS", 0x0087E8FFull, AppCommand::ShowNewCanvasModal);
    tile(group1, row + unit + tileGap, 2, 1, L"OPEN", 0x106EBEFFull, AppCommand::OpenProject);
    tile(group1 + (unit + tileGap) * 2, row + unit + tileGap, 1, 1, L"CLVPIC", 0x0078D7FFull);
    tile(group1, row + (unit + tileGap) * 2, 1, 1, L"RECENT", 0x0087E8FFull);
    tile(group1 + unit + tileGap, row + (unit + tileGap) * 2, 2, 2, L"CLOVER CREATION", 0x005A9EFFull);
    tile(group1, row + (unit + tileGap) * 3, 1, 1, L"BRUSH", 0x00A4E4FFull);
    tile(group1, row + (unit + tileGap) * 4, 2, 1, L"TEXTURE", 0x0078D7FFull);
    tile(group1 + (unit + tileGap) * 2, row + (unit + tileGap) * 4, 1, 1, L"PNG", 0x0099BCFFull);

    tile(group2, row, 1, 1, L"NEWS", 0x0078D7FFull);
    tile(group2 + unit + tileGap, row, 2, 2, L"ANNOUNCEMENT", 0xD13438FFull);
    tile(group2, row + unit + tileGap, 1, 1, L"TIPS", 0x0063B1FFull);
    tile(group2, row + (unit + tileGap) * 2, 2, 1, L"IMAGE PREVIEW", 0x00BCF2FFull);
    tile(group2 + (unit + tileGap) * 2, row + (unit + tileGap) * 2, 1, 1, L"GALLERY", 0x3B5A9AFFull);
    tile(group2, row + (unit + tileGap) * 4, 1, 1, L"OFFICE", 0x881798FFull);
    tile(group2 + unit + tileGap, row + (unit + tileGap) * 4, 2, 1, L"COLOR NOTES", 0x5C8F8BFFull);
}

void AppSceneBuilder::BuildWorkspaceScene(const Size& viewport,
                                          const EditorSession& session,
                                          CanvasController& canvas,
                                          CoreUI::UiScene& scene,
                                          const String& openMenu) {
    const int w = viewport.width;
    const int h = viewport.height;
    constexpr uint32_t visibleOnly = Presentation::UiNodeVisible;
    auto add = [&](Rect rect, String label, AppCommand command = AppCommand::None, uint64_t userData = 0,
                   CoreUI::UiNodeType type = CoreUI::UiNodeType::Button, int z = 20,
                   uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                   String payload = L"",
                   Presentation::IconId iconId = Presentation::IconId::None,
                   CoreUI::IconPlacement placement = CoreUI::IconPlacement::None,
                   String tooltip = L"",
                   bool checked = false,
                   bool disabled = false,
                   String shortcut = L"",
                   bool hasSubmenu = false) {
        AddNode(scene, type, rect, std::move(label), command, userData, std::move(payload), z, flags,
                iconId, placement, std::move(tooltip), checked, disabled, std::move(shortcut), hasSubmenu);
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
        const bool active = openMenu == menu.key;
        if (active) {
            activeMenuLeft = menuX;
            if (openMenu == L"Filter") activeMenuWidth = 316;
            if (openMenu == L"View") activeMenuWidth = 340;
            if (openMenu == L"Tool") activeMenuWidth = 238;
            if (openMenu == L"Window") activeMenuWidth = 248;
            if (openMenu == L"Snap") activeMenuWidth = 256;
        }
        add(Rect(menuX, 0, menuX + menu.width, MenuBarH), menu.label, AppCommand::None, 0,
            CoreUI::UiNodeType::MenuHeader, 80,
            menu.enabled ? Presentation::UiNodeVisible | Presentation::UiNodeInteractive : Presentation::UiNodeVisible,
            menu.key);
        menuX += menu.width;
    }

    int iconX = 6;
    auto icon = [&](String label, AppCommand command, Presentation::IconId iconId, uint64_t data = 0, int width = 25,
                    uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive) {
        add(Rect(iconX, MenuBarH + 4, iconX + width, MenuBarH + 26), std::move(label), command, data,
            CoreUI::UiNodeType::Button, 30, flags, L"", iconId, CoreUI::IconPlacement::Center);
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
        AppCommand::None, 0, CoreUI::UiNodeType::Text, 5, visibleOnly);
    add(Rect(w - 160, MenuBarH + 4, w - 8, MenuBarH + 26), L"Program Manager", AppCommand::GoHome, 0,
        CoreUI::UiNodeType::Button, 32, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
        Presentation::IconId::Home, CoreUI::IconPlacement::Leading);

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
            static_cast<uint64_t>(tools[i]), CoreUI::UiNodeType::ToolbarItem, 25,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"", toolIcons[i], CoreUI::IconPlacement::Center);
        toolY += 60;
    }

    const bool leftSidebarExpanded = canvas.IsLeftSidebarExpanded();
    const bool rightSidebarExpanded = canvas.IsRightSidebarExpanded();
    add(Rect(ToolBarW + (leftSidebarExpanded ? LeftPanelW - 18 : 0), TopBarH + 8,
             ToolBarW + (leftSidebarExpanded ? LeftPanelW : 18), TopBarH + 86),
        leftSidebarExpanded ? L"<" : L">", AppCommand::ToggleLeftSidebar, 0,
        CoreUI::UiNodeType::Button, 36);
    add(Rect(w - (rightSidebarExpanded ? RightPanelW : 18), TopBarH + 8,
             w - (rightSidebarExpanded ? RightPanelW - 18 : 0), TopBarH + 86),
        rightSidebarExpanded ? L">" : L"<", AppCommand::ToggleRightSidebar, 0,
        CoreUI::UiNodeType::Button, 36);

    const int leftPanelLeft = ToolBarW + 4;
    const int leftPanelRight = ToolBarW + LeftPanelW - 4;
    const int panelInnerLeft = leftPanelLeft + 8;
    const int colorTop = TopBarH + 4;
    const bool showColorPanel = leftSidebarExpanded && canvas.IsPanelVisible(WorkspacePanelId::Color);
    const bool showBrushControlPanel = leftSidebarExpanded && canvas.IsPanelVisible(WorkspacePanelId::BrushControl);
    const bool showBrushPresetPanel = leftSidebarExpanded && canvas.IsPanelVisible(WorkspacePanelId::BrushPresets);
    const bool showNavigatorPanel = rightSidebarExpanded && canvas.IsPanelVisible(WorkspacePanelId::Navigator);
    const bool showLayerPanel = rightSidebarExpanded && canvas.IsPanelVisible(WorkspacePanelId::Layer);
    const bool showBrushSizePanel = rightSidebarExpanded && canvas.IsPanelVisible(WorkspacePanelId::BrushSize);
    const int colorFieldSize = std::min(216, leftPanelRight - panelInnerLeft - 38);
    const int colorFieldLeft = panelInnerLeft + 42;
    const int colorFieldTop = colorTop + 28;
    if (showColorPanel) {
        add(Rect(colorFieldLeft, colorFieldTop, colorFieldLeft + colorFieldSize, colorFieldTop + colorFieldSize),
            L"Color field", AppCommand::None, 0, CoreUI::UiNodeType::ColorField, 24);
        add(Rect(colorFieldLeft + colorFieldSize + 6, colorFieldTop, colorFieldLeft + colorFieldSize + 24, colorFieldTop + colorFieldSize),
            L"Hue", AppCommand::None, 0, CoreUI::UiNodeType::HueStrip, 24);
    }

    const uint64_t swatches[] = {
        SwatchBlack, SwatchWhite, SwatchRed, SwatchBlue, SwatchGreen, SwatchYellow,
        SwatchPurple, 0x00A4E4FFull, 0xFF8C00FFull, 0x107C10FFull, 0x5C2D91FFull, SwatchTransparent
    };
    if (showColorPanel) {
        for (int i = 0; i < 12; ++i) {
            const int sx = panelInnerLeft + (i % 6) * 34;
            const int sy = colorFieldTop + colorFieldSize + 12 + (i / 6) * 28;
            add(Rect(sx, sy, sx + 26, sy + 20), L"", AppCommand::SetColor, swatches[i], CoreUI::UiNodeType::Swatch, 24);
        }
    }

    const int brushPreviewTop = colorFieldTop + colorFieldSize + 76;
    const int brushControlTop = brushPreviewTop + 128;
    const int sliderLeft = panelInnerLeft;
    const int sliderRight = leftPanelRight - 8;
    if (showBrushControlPanel) {
        add(Rect(sliderLeft, brushControlTop + 30, sliderRight, brushControlTop + 54), L"Size", AppCommand::SetBrushParam,
            PackBrushParam(BrushParamId::Size, static_cast<uint16_t>(std::clamp<int>(static_cast<int>(canvas.GetBrushSize()), 1, 100))),
            CoreUI::UiNodeType::Slider, 24);
        add(Rect(sliderLeft, brushControlTop + 58, sliderRight, brushControlTop + 82), L"Opacity", AppCommand::SetBrushParam,
            PackBrushParam(BrushParamId::Opacity, static_cast<uint16_t>(canvas.GetBrushOpacity() * 100)), CoreUI::UiNodeType::Slider, 24);
        add(Rect(sliderLeft, brushControlTop + 86, sliderRight, brushControlTop + 110), L"Flow", AppCommand::SetBrushParam,
            PackBrushParam(BrushParamId::Flow, static_cast<uint16_t>(canvas.GetBrushFlow() * 100)), CoreUI::UiNodeType::Slider, 24);
        add(Rect(sliderLeft, brushControlTop + 114, sliderRight, brushControlTop + 138), L"Spacing", AppCommand::SetBrushParam,
            PackBrushParam(BrushParamId::Spacing, static_cast<uint16_t>(canvas.GetBrushSpacing() * 100)), CoreUI::UiNodeType::Slider, 24);
        add(Rect(sliderLeft, brushControlTop + 142, sliderRight, brushControlTop + 166), L"Wet Mix", AppCommand::SetBrushParam,
            PackBrushParam(BrushParamId::WetMix, static_cast<uint16_t>(canvas.GetBrushWetMix() * 100)), CoreUI::UiNodeType::Slider, 24);
    }

    const int tipTop = brushControlTop + 178;
    if (showBrushControlPanel) {
        add(Rect(sliderLeft, tipTop, sliderLeft + 72, tipTop + 24), L"Hard", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::RoundHard));
        add(Rect(sliderLeft + 78, tipTop, sliderLeft + 150, tipTop + 24), L"Soft", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::RoundSoft));
        add(Rect(sliderLeft + 156, tipTop, sliderLeft + 228, tipTop + 24), L"Flat", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::Flat));
        add(Rect(sliderLeft, tipTop + 30, sliderLeft + 104, tipTop + 54), L"Bristle", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::Bristle));
        add(Rect(sliderLeft + 110, tipTop + 30, sliderLeft + 214, tipTop + 54), L"Texture", AppCommand::SetBrushTip,
            static_cast<uint64_t>(Render::BrushTipType::Texture));
    }

    struct PresetDef {
        const wchar_t* label;
        uint16_t size;
        Render::BrushTipType tip;
        uint64_t accent;
    };
    const PresetDef presets[] = {
        { L"15  Pen", 15, Render::BrushTipType::RoundHard, SwatchBlue },
        { L"8   Pen Soft", 8, Render::BrushTipType::RoundSoft, 0x7CC7FFFFull },
        { L"20  Mapping Pen", 20, Render::BrushTipType::RoundHard, SwatchRed },
        { L"30  Kabura Pen", 30, Render::BrushTipType::Flat, 0xFF8C00FFull },
        { L"40  Watercolor", 40, Render::BrushTipType::Bristle, SwatchGreen },
        { L"60  Texture Pen", 60, Render::BrushTipType::Texture, SwatchPurple }
    };
    const int presetTop = std::max(tipTop + 72, h - StatusBarH - 198);
    if (showBrushPresetPanel) {
        for (int i = 0; i < 6; ++i) {
            add(Rect(panelInnerLeft, presetTop + i * 27, leftPanelRight - 8, presetTop + i * 27 + 25),
                presets[i].label, AppCommand::SetBrushPreset,
                PackBrushPreset(presets[i].size, static_cast<uint16_t>(presets[i].tip)),
                CoreUI::UiNodeType::BrushPresetItem, 24, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                std::to_wstring(presets[i].accent));
        }
    }

    const int navLeft = w - RightPanelW + 8;
    const int navRight = w - 8;
    if (showNavigatorPanel) {
        add(Rect(navLeft + 8, TopBarH + 34, navRight - 8, TopBarH + 154), L"Navigator Preview",
            AppCommand::None, 0, CoreUI::UiNodeType::Text, 10, visibleOnly);
        add(Rect(navLeft + 8, TopBarH + 162, navLeft + 58, TopBarH + 186), L"50%", AppCommand::SetZoomPreset, 50,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::ZoomOut, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 64, TopBarH + 162, navLeft + 116, TopBarH + 186), L"100%", AppCommand::SetZoomPreset, 100,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::ZoomReset, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 122, TopBarH + 162, navLeft + 174, TopBarH + 186), L"200%", AppCommand::SetZoomPreset, 200,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::ZoomIn, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 180, TopBarH + 162, navRight - 8, TopBarH + 186), L"Reset", AppCommand::ResetView,
            0, CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::ArrowsMaximize, CoreUI::IconPlacement::Leading);
    }

    auto* lm = canvas.GetLayerManager();
    uint8_t activeOpacity = 255;
    if (lm) {
        if (auto active = lm->GetActiveLayer()) {
            activeOpacity = active->GetOpacity();
        }
    }
    const int layerTop = TopBarH + 210;
    if (showLayerPanel) {
        add(Rect(navLeft + 8, layerTop + 30, navRight - 8, layerTop + 54), L"Opacity", AppCommand::SetLayerOpacity,
            activeOpacity, CoreUI::UiNodeType::Slider, 24);
        add(Rect(navLeft + 8, layerTop + 62, navLeft + 132, layerTop + 86), L"Normal", AppCommand::SetBlendMode,
            static_cast<uint64_t>(BlendMode::Normal));
        add(Rect(navLeft + 138, layerTop + 62, navRight - 8, layerTop + 86), L"Multiply", AppCommand::SetBlendMode,
            static_cast<uint64_t>(BlendMode::Multiply));
        add(Rect(navLeft + 8, layerTop + 92, navLeft + 88, layerTop + 116), L"Alpha", AppCommand::ToggleProtectAlpha);
        add(Rect(navLeft + 94, layerTop + 92, navLeft + 166, layerTop + 116), L"Clip", AppCommand::ShowUnavailable, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            L"", Presentation::IconId::None, CoreUI::IconPlacement::None, L"", false, true);
        add(Rect(navLeft + 172, layerTop + 92, navRight - 8, layerTop + 116), L"Lock", AppCommand::ToggleLayerLock);
    }

    const int layerToolsY = h - StatusBarH - 226;
    if (lm && showLayerPanel) {
        int y = layerTop + 128;
        for (size_t i = 0; i < lm->GetLayerCount() && y + 54 < layerToolsY - 8; ++i) {
            auto layer = lm->GetLayer(i);
            String label = layer ? layer->GetName() : L"Layer";
            add(Rect(navLeft + 8, y, navRight - 8, y + 52), label, AppCommand::SelectLayer, static_cast<uint64_t>(i),
                CoreUI::UiNodeType::LayerItem, 25);
            y += 56;
        }
    }
    if (showLayerPanel) {
        add(Rect(navLeft + 8, layerToolsY, navLeft + 40, layerToolsY + 24), L"Add", AppCommand::AddLayer, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::Plus, CoreUI::IconPlacement::Center);
        add(Rect(navLeft + 46, layerToolsY, navLeft + 84, layerToolsY + 24), L"Text", AppCommand::ShowUnavailable, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::TextPlus, CoreUI::IconPlacement::Center, L"", false, true);
        add(Rect(navLeft + 90, layerToolsY, navLeft + 132, layerToolsY + 24), L"Dup", AppCommand::DuplicateLayer, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::Copy, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 138, layerToolsY, navLeft + 190, layerToolsY + 24), L"Merge", AppCommand::MergeLayerDown, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::Merge, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 196, layerToolsY, navLeft + 238, layerToolsY + 24), L"Solo", AppCommand::ToggleSolo, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::Eye, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 244, layerToolsY, navRight - 8, layerToolsY + 24), L"Del", AppCommand::DeleteLayer, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::Trash, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 8, layerToolsY + 30, navLeft + 82, layerToolsY + 54), L"Visible", AppCommand::ToggleLayerVisibility, 0,
            CoreUI::UiNodeType::Button, 20, Presentation::UiNodeVisible | Presentation::UiNodeInteractive, L"",
            Presentation::IconId::Eye, CoreUI::IconPlacement::Leading);
        add(Rect(navLeft + 88, layerToolsY + 30, navLeft + 174, layerToolsY + 54), L"Opacity 50", AppCommand::SetLayerOpacity, 128);
        add(Rect(navLeft + 180, layerToolsY + 30, navRight - 8, layerToolsY + 54), L"Opacity 100", AppCommand::SetLayerOpacity, 255);
    }

    const int sizeTop = h - StatusBarH - 156;
    const uint16_t sizes[] = { 1, 3, 5, 7, 10, 12, 15, 20, 25, 30, 40, 50, 70, 100 };
    if (showBrushSizePanel) {
        for (int i = 0; i < 14; ++i) {
            const int cx = navLeft + 10 + (i % 7) * 41;
            const int cy = sizeTop + 30 + (i / 7) * 50;
            add(Rect(cx, cy, cx + 34, cy + 42), std::to_wstring(sizes[i]), AppCommand::SetBrushParam,
                PackBrushParam(BrushParamId::Size, sizes[i]), CoreUI::UiNodeType::BrushSizeChip, 24);
        }
    }

    auto addMenuItem = [&](int index, String label, AppCommand command = AppCommand::None, uint64_t data = 0,
                           bool enabled = true, String payload = L"", bool checked = false, bool hasSubmenu = false) {
        const int itemTop = MenuBarH + 8 + index * 28;
        String shortcut;
        const size_t sep = label.find(L'|');
        if (sep != String::npos) {
            shortcut = label.substr(sep + 1);
            label = label.substr(0, sep);
        }
        const AppCommand finalCommand = enabled ? command : AppCommand::ShowUnavailable;
        add(Rect(activeMenuLeft, itemTop, activeMenuLeft + activeMenuWidth, itemTop + 26),
            std::move(label), finalCommand, data, CoreUI::UiNodeType::MenuItem, 120,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
            std::move(payload), Presentation::IconId::None, CoreUI::IconPlacement::None, L"",
            checked, !enabled, std::move(shortcut), hasSubmenu);
    };
    auto addMenuSeparator = [&](int index) {
        const int itemTop = MenuBarH + 8 + index * 28;
        add(Rect(activeMenuLeft + 12, itemTop + 13, activeMenuLeft + activeMenuWidth - 12, itemTop + 14),
            L"", AppCommand::None, 0, CoreUI::UiNodeType::MenuSeparator, 119, visibleOnly);
    };

    if (activeMenuLeft >= 0) {
        int panelBottom = MenuBarH + 350;
        if (openMenu == L"View") panelBottom = MenuBarH + 480;
        if (openMenu == L"Tool") panelBottom = MenuBarH + 530;
        if (openMenu == L"Window") panelBottom = MenuBarH + 420;
        if (openMenu == L"Snap") panelBottom = MenuBarH + 360;
        add(Rect(activeMenuLeft, MenuBarH, activeMenuLeft + activeMenuWidth, std::min(h - StatusBarH, panelBottom)),
            L"", AppCommand::None, 0, CoreUI::UiNodeType::Panel, 118,
            Presentation::UiNodeVisible | Presentation::UiNodeInteractive);
        if (openMenu == L"File") {
            addMenuItem(0, L"New(N)...|Ctrl+N", AppCommand::ShowNewCanvasModal);
            addMenuItem(1, L"Open(O)...|Ctrl+O", AppCommand::OpenProject);
            int fileIndex = 2;
            const auto recent = session.GetRecentFiles();
            if (recent.empty()) {
                addMenuItem(fileIndex++, L"Open Recent File(R)|", AppCommand::None, 0, false, L"", false, true);
            } else {
                addMenuItem(fileIndex++, L"Open Recent File(R)|", AppCommand::None, 0, false, L"", false, true);
                const int recentLimit = std::min<int>(3, static_cast<int>(recent.size()));
                for (int i = 0; i < recentLimit; ++i) {
                    addMenuItem(fileIndex++, CompactPathLabel(FileNameFromPath(recent[static_cast<size_t>(i)]), 24) + L"|",
                                AppCommand::OpenRecentProject, 0, true, recent[static_cast<size_t>(i)]);
                }
            }
            addMenuSeparator(fileIndex++);
            addMenuItem(fileIndex++, L"Save(S)|Ctrl+S", AppCommand::SaveProject);
            addMenuItem(fileIndex++, L"Save As(A)...|Ctrl+Shift+S", AppCommand::SaveProjectAs);
            addMenuItem(fileIndex++, L"Export...|", AppCommand::ExportPng);
            addMenuSeparator(fileIndex++);
            addMenuItem(fileIndex++, L"Environment Settings(K)...|Ctrl+K", AppCommand::ShowSettingsModal);
            addMenuItem(fileIndex++, L"Close Project|Ctrl+Alt+W", AppCommand::CloseWorkspace);
        } else if (openMenu == L"Edit") {
            addMenuItem(0, L"Undo(Z)|Ctrl+Z", AppCommand::Undo);
            addMenuItem(1, L"Redo(Y)|Ctrl+Y", AppCommand::Redo);
            addMenuSeparator(2);
            addMenuItem(3, L"Cut(T)|Ctrl+X", AppCommand::None, 0, false);
            addMenuItem(4, L"Copy(C)|Ctrl+C", AppCommand::None, 0, false);
            addMenuItem(5, L"Paste(V)|Ctrl+V", AppCommand::None, 0, false);
            addMenuSeparator(6);
            addMenuItem(7, L"Canvas Size(S)...|Ctrl+Alt+C", AppCommand::ShowCanvasSizeModal);
            addMenuItem(8, L"Rotate Left(L)|", AppCommand::RotateLayerLeft);
            addMenuItem(9, L"Rotate Right(R)|", AppCommand::RotateLayerRight);
            addMenuItem(10, L"Flip Horizontal(H)|", AppCommand::FlipLayerHorizontal);
            addMenuItem(11, L"Flip Vertical(V)|", AppCommand::FlipLayerVertical);
        } else if (openMenu == L"Layer") {
            addMenuItem(0, L"New Raster Layer|", AppCommand::AddLayer);
            addMenuItem(1, L"New Text Layer|", AppCommand::None, 0, false);
            addMenuItem(2, L"Duplicate Layer|", AppCommand::DuplicateLayer);
            addMenuItem(3, L"Merge Layer Down|", AppCommand::MergeLayerDown);
            addMenuItem(4, L"Delete Layer|", AppCommand::DeleteLayer);
            addMenuSeparator(5);
            addMenuItem(6, L"Toggle Visible|", AppCommand::ToggleLayerVisibility);
            addMenuItem(7, L"Toggle Lock|", AppCommand::ToggleLayerLock);
            addMenuItem(8, L"Protect Alpha|", AppCommand::ToggleProtectAlpha);
            addMenuItem(9, L"Solo Layer|", AppCommand::ToggleSolo);
        } else if (openMenu == L"Filter") {
            addMenuItem(0, L"Levels(L)...|Ctrl+L", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::BrightnessContrast));
            addMenuItem(1, L"Hue(H)...|Ctrl+U", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::HueSaturation));
            addMenuItem(2, L"Tone Curve(T)...|", AppCommand::None, 0, false);
            addMenuItem(3, L"Channel Operation(O)...|", AppCommand::None, 0, false);
            addMenuSeparator(4);
            addMenuItem(5, L"Reverse Color|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Invert));
            addMenuItem(6, L"Change Transparent Color|", AppCommand::None, 0, false);
            addMenuSeparator(7);
            addMenuItem(8, L"Unsharp Mask...|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Sharpen, 40));
            addMenuItem(9, L"Gaussian Blur(G)...|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::GaussianBlur, 2));
            addMenuItem(10, L"Threshold...|", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Threshold, 128));
            addMenuItem(11, L"Lens Blur...|", AppCommand::None, 0, false);
            addMenuItem(12, L"Mosaic(S)...|", AppCommand::None, 0, false);
        } else if (openMenu == L"Select") {
            addMenuItem(0, L"Select All(A)|Ctrl+A", AppCommand::SelectAll);
            addMenuItem(1, L"Clear Selection(D)|Ctrl+D", AppCommand::ClearSelection);
            addMenuItem(2, L"Invert Selection(I)|Ctrl+Shift+I", AppCommand::InvertSelection);
            addMenuSeparator(3);
            addMenuItem(4, L"Rectangle Selection Tool|", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::RectSelect));
            addMenuItem(5, L"Lasso Selection Tool|", AppCommand::None, 0, false);
            addMenuItem(6, L"Magic Wand Tool|", AppCommand::None, 0, false);
        } else if (openMenu == L"Snap") {
            addMenuItem(0, L"Off(O)|1", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Off), true, L"",
                        canvas.GetSnapMode() == SnapModeId::Off);
            addMenuItem(1, L"Parallel(P)|2", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Parallel), true, L"",
                        canvas.GetSnapMode() == SnapModeId::Parallel);
            addMenuItem(2, L"Crisscross(C)|3", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Crisscross), true, L"",
                        canvas.GetSnapMode() == SnapModeId::Crisscross);
            addMenuItem(3, L"Vanishing Point(V)|4", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::VanishingPoint), true, L"",
                        canvas.GetSnapMode() == SnapModeId::VanishingPoint);
            addMenuItem(4, L"Radial(R)|5", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Radial), true, L"",
                        canvas.GetSnapMode() == SnapModeId::Radial);
            addMenuItem(5, L"Circle(E)|6", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Circle), true, L"",
                        canvas.GetSnapMode() == SnapModeId::Circle);
            addMenuItem(6, L"Curve(K)|7", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::Curve), true, L"",
                        canvas.GetSnapMode() == SnapModeId::Curve);
            addMenuItem(7, L"Curved Line (ellipse)(L)|8", AppCommand::SetSnapMode, static_cast<uint64_t>(SnapModeId::CurvedLineEllipse), true, L"",
                        canvas.GetSnapMode() == SnapModeId::CurvedLineEllipse);
            addMenuSeparator(8);
            addMenuItem(9, L"Draw Curve|", AppCommand::None, 0, false);
            addMenuItem(10, L"Draw Curve (Fade In/Out)|", AppCommand::None, 0, false);
            addMenuSeparator(11);
            addMenuItem(12, L"Save Snap(S)...|", AppCommand::None, 0, false);
        } else if (openMenu == L"View") {
            addMenuItem(0, L"Zoom In(I)|Ctrl++", AppCommand::ZoomIn);
            addMenuItem(1, L"Zoom Out(O)|Ctrl+-", AppCommand::ZoomOut);
            addMenuItem(2, L"Fit to Window Size(F)|Ctrl+0", AppCommand::FitCanvas);
            addMenuItem(3, L"100%|Ctrl+1", AppCommand::SetZoomPreset, 100,
                        true, L"", std::abs(canvas.GetZoom() - 1.0f) < 0.01f);
            addMenuItem(4, L"200%|", AppCommand::SetZoomPreset, 200,
                        true, L"", std::abs(canvas.GetZoom() - 2.0f) < 0.01f);
            addMenuSeparator(5);
            addMenuItem(6, L"Rotate Left(L)|Left", AppCommand::None, 0, false);
            addMenuItem(7, L"Release Rotate/Flip(S)|Up", AppCommand::ResetView);
            addMenuItem(8, L"Rotate Right(R)|Right", AppCommand::None, 0, false);
            addMenuItem(9, L"Flip Horizontally(H)|Down", AppCommand::None, 0, false);
            addMenuSeparator(10);
            addMenuItem(11, L"Use and display the color profile.|", AppCommand::ToggleViewOption,
                        static_cast<uint64_t>(ViewOptionId::ColorProfileDisplay), true, L"",
                        canvas.IsViewOptionEnabled(ViewOptionId::ColorProfileDisplay));
            addMenuItem(12, L"CMYK Soft Proof|", AppCommand::None, 0, false);
            addMenuItem(13, L"Color Management Settings...|", AppCommand::ShowSettingsModal);
            addMenuSeparator(14);
            addMenuItem(15, L"Grid(G)|Ctrl+G", AppCommand::ToggleViewOption,
                        static_cast<uint64_t>(ViewOptionId::Grid), true, L"",
                        canvas.IsViewOptionEnabled(ViewOptionId::Grid));
            addMenuItem(16, L"Pixel Grid(P)|", AppCommand::ToggleViewOption,
                        static_cast<uint64_t>(ViewOptionId::PixelGrid), true, L"",
                        canvas.IsViewOptionEnabled(ViewOptionId::PixelGrid));
            addMenuItem(17, L"Grid Settings...|", AppCommand::None, 0, false);
            addMenuSeparator(18);
            addMenuItem(19, L"Transparent Background(T)|", AppCommand::ToggleViewOption,
                        static_cast<uint64_t>(ViewOptionId::TransparentBackground), true, L"",
                        canvas.IsViewOptionEnabled(ViewOptionId::TransparentBackground));
            addMenuItem(20, L"Background Color(D)...|", AppCommand::None, 0, false);
        } else if (openMenu == L"Tool") {
            addMenuItem(0, L"Brush|B", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Brush), true, L"", canvas.GetTool() == ToolType::Brush);
            addMenuItem(1, L"Eraser|E", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Eraser), true, L"", canvas.GetTool() == ToolType::Eraser);
            addMenuItem(2, L"Eraser(Lasso)|", AppCommand::None, 0, false);
            addMenuItem(3, L"Shape Brush|U", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Shape), true, L"", canvas.GetTool() == ToolType::Shape);
            addMenuItem(4, L"Dot Brush|Shift+B", AppCommand::None, 0, false);
            addMenuItem(5, L"Move|V", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Move), true, L"", canvas.GetTool() == ToolType::Move);
            addMenuSeparator(6);
            addMenuItem(7, L"Fill|N", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Fill), true, L"", canvas.GetTool() == ToolType::Fill);
            addMenuItem(8, L"Bucket|G", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Fill), true, L"", canvas.GetTool() == ToolType::Fill);
            addMenuItem(9, L"Gradient|Shift+G", AppCommand::None, 0, false);
            addMenuSeparator(10);
            addMenuItem(11, L"Select|M", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::RectSelect), true, L"", canvas.GetTool() == ToolType::RectSelect);
            addMenuItem(12, L"Lasso|L", AppCommand::None, 0, false);
            addMenuItem(13, L"MagicWand|W", AppCommand::None, 0, false);
            addMenuItem(14, L"Select Pen|S", AppCommand::None, 0, false);
            addMenuItem(15, L"Text|T", AppCommand::None, 0, false);
            addMenuItem(16, L"Eyedropper|I", AppCommand::SelectTool, static_cast<uint64_t>(ToolType::Eyedropper), true, L"", canvas.GetTool() == ToolType::Eyedropper);
        } else if (openMenu == L"Color") {
            addMenuItem(0, L"Transparent(A)|Shift+Z", AppCommand::SetColor, SwatchTransparent, true, L"",
                        canvas.GetColor().a == 0);
            addMenuItem(1, L"Swap Fore/BG(S)|X", AppCommand::None, 0, false);
            addMenuItem(2, L"Initialize(I)|D", AppCommand::SetColor, SwatchBlack);
            addMenuSeparator(3);
            addMenuItem(4, L"Color Bar(B)|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Color),
                        true, L"", canvas.IsPanelVisible(WorkspacePanelId::Color));
            addMenuItem(5, L"Color Wheel(W)|", AppCommand::None, 0, false);
            addMenuItem(6, L"Color Wheel Triangle|", AppCommand::None, 0, false);
            addMenuSeparator(7);
            addMenuItem(8, L"Lock Palette|", AppCommand::None, 0, false);
        } else if (openMenu == L"Window") {
            addMenuItem(0, L"Initialize(I)...|", AppCommand::InitializeLayout);
            addMenuItem(1, L"Show/Hide(S)|Tab", AppCommand::None, 0, false);
            addMenuSeparator(2);
            addMenuItem(3, L"Color|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Color), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::Color));
            addMenuItem(4, L"Palette|", AppCommand::None, 0, false);
            addMenuItem(5, L"Brush Preview|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushPreview), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::BrushPreview));
            addMenuItem(6, L"Brush Control|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushControl), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::BrushControl));
            addMenuItem(7, L"Brush|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushPresets), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::BrushPresets));
            addMenuItem(8, L"Navigator|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Navigator), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::Navigator));
            addMenuItem(9, L"Layer|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::Layer), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::Layer));
            addMenuItem(10, L"Reference|", AppCommand::None, 0, false);
            addMenuItem(11, L"Brush Size|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::BrushSize), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::BrushSize));
            addMenuSeparator(12);
            addMenuItem(13, L"Keyboard Support|", AppCommand::None, 0, false);
            addMenuItem(14, L"Status Bar|", AppCommand::TogglePanel, static_cast<uint64_t>(WorkspacePanelId::StatusBar), true, L"",
                        canvas.IsPanelVisible(WorkspacePanelId::StatusBar));
        } else {
            addMenuItem(0, L"This panel is reserved for MVP expansion|", AppCommand::None, 0, false);
        }
    }
}

void AppSceneBuilder::BuildModalScene(ModalKind modal, const Size& viewport, const RgbaFrame& frame, CoreUI::UiScene& scene) {
    if (modal == ModalKind::None) {
        return;
    }

    CoreUI::UiNode blocker;
    blocker.type = CoreUI::UiNodeType::Panel;
    blocker.bounds = FullRect(frame);
    blocker.command = static_cast<uint32_t>(AppCommand::None);
    blocker.zOrder = 90;
    blocker.accessibilityLabel = L"Modal blocker";
    const auto blockerId = scene.AddNode(std::move(blocker));
    scene.PushModal(blockerId);

    const int modalW = 420;
    const int modalH = 286;
    const int left = std::max(20, viewport.width / 2 - modalW / 2);
    const int top = std::max(20, viewport.height / 2 - modalH / 2);
    auto addModalButton = [&](int y, String label, AppCommand command, uint64_t userData = 0) {
        CoreUI::UiNode node;
        node.type = CoreUI::UiNodeType::Button;
        node.parentId = blockerId;
        node.bounds = Rect(left + 30, top + y, left + modalW - 30, top + y + 36);
        node.label = std::move(label);
        node.accessibilityLabel = node.label;
        node.command = static_cast<uint32_t>(command);
        node.userData = userData;
        node.zOrder = 100;
        scene.AddNode(std::move(node));
    };

    if (modal == ModalKind::NewCanvas) {
        addModalButton(82, L"ILLUSTRATION 1600 x 1000", AppCommand::CreateCanvasPreset, PackCanvasPreset(1600, 1000));
        addModalButton(128, L"HD CANVAS 1920 x 1080", AppCommand::CreateCanvasPreset, PackCanvasPreset(1920, 1080));
        addModalButton(174, L"PRINT A4 2480 x 3508", AppCommand::CreateCanvasPreset, PackCanvasPreset(2480, 3508));
        addModalButton(226, L"CANCEL", AppCommand::CloseModal);
    } else if (modal == ModalKind::Filters) {
        addModalButton(82, L"INVERT", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Invert));
        addModalButton(128, L"THRESHOLD 128", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Threshold, 128));
        addModalButton(174, L"GAUSSIAN BLUR", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::GaussianBlur, 2));
        addModalButton(220, L"SHARPEN", AppCommand::ApplyFilter, PackFilterCommand(FilterCommandId::Sharpen, 40));
        addModalButton(266, L"CANCEL", AppCommand::CloseModal);
    } else if (modal == ModalKind::TextLayer) {
        addModalButton(82, L"CREATE DEFAULT TEXT 36 PX", AppCommand::CreateTextLayer, 0);
        auto* node = scene.Find(scene.Nodes().empty() ? 0 : scene.Nodes().back().id);
        if (node) node->payload = L"CloverPic|36";
        addModalButton(128, L"CREATE TITLE TEXT 64 PX", AppCommand::CreateTextLayer, 0);
        node = scene.Find(scene.Nodes().empty() ? 0 : scene.Nodes().back().id);
        if (node) node->payload = L"Clover Creation|64";
        addModalButton(174, L"CREATE SMALL NOTE 24 PX", AppCommand::CreateTextLayer, 0);
        node = scene.Find(scene.Nodes().empty() ? 0 : scene.Nodes().back().id);
        if (node) node->payload = L"New Text|24";
        addModalButton(226, L"CANCEL", AppCommand::CloseModal);
    }
}

} // namespace CloverPic::Core
