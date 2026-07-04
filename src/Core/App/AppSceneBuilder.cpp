#include "Core/App/AppSceneBuilder.h"
#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"
#include <algorithm>
#include <cwctype>

namespace CloverPic::Core {

namespace {

constexpr int TopBarH = 34;
constexpr int StatusBarH = 28;
constexpr int ToolBarW = 48;
constexpr int LeftPanelW = 236;
constexpr int RightPanelW = 252;

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

void AddNode(CoreUI::UiScene& scene,
             CoreUI::UiNodeType type,
             const Rect& bounds,
             String label,
             AppCommand command = AppCommand::None,
             uint64_t userData = 0,
             String payload = L"",
             int z = 10,
             uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive) {
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
    scene.AddNode(std::move(node));
}

} // namespace

void AppSceneBuilder::Build(AppScreen screen,
                            ModalKind modal,
                            const Size& viewport,
                            const RgbaFrame& frame,
                            const EditorSession& session,
                            CanvasController& canvas,
                            const String& homeSearchQuery,
                            CoreUI::UiScene& scene) {
    scene.Clear();
    if (screen == AppScreen::Home) {
        BuildHomeScene(viewport, session, homeSearchQuery, scene);
    } else {
        BuildWorkspaceScene(viewport, canvas, scene);
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
            L"SETTINGS", AppCommand::None, 0, L"", 8);
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
    tile(group1 + (unit + tileGap) * 2, row + unit + tileGap, 1, 1, L"VVP", 0x0078D7FFull);
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

void AppSceneBuilder::BuildWorkspaceScene(const Size& viewport, CanvasController& canvas, CoreUI::UiScene& scene) {
    const int w = viewport.width;
    const int h = viewport.height;
    auto addButton = [&](Rect rect, String label, AppCommand command, uint64_t userData = 0, int z = 20,
                         CoreUI::UiNodeType type = CoreUI::UiNodeType::Button) {
        CoreUI::UiNode node;
        node.type = type;
        node.bounds = rect;
        node.label = std::move(label);
        node.accessibilityLabel = node.label;
        node.command = static_cast<uint32_t>(command);
        node.userData = userData;
        node.zOrder = z;
        scene.AddNode(std::move(node));
    };

    addButton(Rect(8, 5, 58, 29), L"HOME", AppCommand::GoHome);
    addButton(Rect(66, 5, 116, 29), L"SAVE", AppCommand::SaveProject);
    addButton(Rect(124, 5, 184, 29), L"EXPORT", AppCommand::ExportPng);
    addButton(Rect(192, 5, 236, 29), L"UNDO", AppCommand::Undo);
    addButton(Rect(244, 5, 286, 29), L"REDO", AppCommand::Redo);

    addButton(Rect(6, TopBarH + 8, ToolBarW - 6, TopBarH + 44), L"B", AppCommand::SelectTool,
              static_cast<uint64_t>(ToolType::Brush), 20, CoreUI::UiNodeType::ToolbarItem);
    addButton(Rect(6, TopBarH + 50, ToolBarW - 6, TopBarH + 86), L"E", AppCommand::SelectTool,
              static_cast<uint64_t>(ToolType::Eraser), 20, CoreUI::UiNodeType::ToolbarItem);

    addButton(Rect(ToolBarW + 16, TopBarH + 58, ToolBarW + 64, TopBarH + 90), L"BLACK",
              AppCommand::SetColor, SwatchBlack, 20, CoreUI::UiNodeType::Swatch);
    addButton(Rect(ToolBarW + 72, TopBarH + 58, ToolBarW + 120, TopBarH + 90), L"RED",
              AppCommand::SetColor, SwatchRed, 20, CoreUI::UiNodeType::Swatch);
    addButton(Rect(ToolBarW + 128, TopBarH + 58, ToolBarW + 176, TopBarH + 90), L"BLUE",
              AppCommand::SetColor, SwatchBlue, 20, CoreUI::UiNodeType::Swatch);
    addButton(Rect(w - RightPanelW + 12, h - StatusBarH - 92, w - 132, h - StatusBarH - 62), L"ADD", AppCommand::AddLayer);
    addButton(Rect(w - 122, h - StatusBarH - 92, w - 14, h - StatusBarH - 62), L"DELETE", AppCommand::DeleteLayer);
    addButton(Rect(w - RightPanelW + 12, h - StatusBarH - 54, w - 14, h - StatusBarH - 24), L"TOGGLE VIS", AppCommand::ToggleLayerVisibility);

    auto* lm = canvas.GetLayerManager();
    if (lm) {
        const int panelLeft = w - RightPanelW + 20;
        int y = TopBarH + 220;
        for (size_t i = 0; i < lm->GetLayerCount() && y + 30 < h - StatusBarH - 110; ++i) {
            auto layer = lm->GetLayer(i);
            addButton(Rect(panelLeft, y, w - 20, y + 30), layer ? layer->GetName() : L"LAYER",
                      AppCommand::SelectLayer, static_cast<uint64_t>(i), 25, CoreUI::UiNodeType::LayerItem);
            y += 34;
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
    }
}

} // namespace CloverPic::Core
