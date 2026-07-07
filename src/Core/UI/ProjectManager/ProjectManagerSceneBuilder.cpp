#include "Core/UI/ProjectManager/ProjectManagerSceneBuilder.h"
#include "Core/App/AppCommand.h"
#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"
#include <algorithm>
#include <cwctype>

namespace CloverPic::Core {

namespace {

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
    return scene.AddNode(std::move(node));
}

} // namespace

void ProjectManagerSceneBuilder::BuildHomeScene(const Size& viewport,
                                                const EditorSession& session,
                                                const String& searchQuery,
                                                CoreUI::UiScene& scene) {
    scene.Clear();
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
            searchQuery.empty() ? L"SEARCH RECENT PROJECTS" : searchQuery, AppCommand::FocusHomeSearch, 0,
            ProjectManagerUiState::FieldPayload(ProjectManagerActiveField::Search), 20);

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

} // namespace CloverPic::Core
