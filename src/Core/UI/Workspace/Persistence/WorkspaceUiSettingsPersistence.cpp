#include "Core/UI/Workspace/Persistence/WorkspaceUiSettingsPersistence.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace CloverPic::Core {

namespace {

const char* PanelKey(WorkspacePanelId id) {
    switch (id) {
        case WorkspacePanelId::Color: return "Color";
        case WorkspacePanelId::BrushPreview: return "BrushPreview";
        case WorkspacePanelId::BrushControl: return "BrushControl";
        case WorkspacePanelId::BrushPresets: return "BrushPresets";
        case WorkspacePanelId::Navigator: return "Navigator";
        case WorkspacePanelId::Layer: return "Layer";
        case WorkspacePanelId::BrushSize: return "BrushSize";
        case WorkspacePanelId::StatusBar: return "StatusBar";
    }
    return "StatusBar";
}

std::string PanelScopeValue(const std::string& payload, const std::string& panelName, const std::string& key, const std::string& fallback = "") {
    const std::string panelNeedle = "\"" + panelName + "\":{";
    const size_t panelPos = payload.find(panelNeedle);
    if (panelPos == std::string::npos) {
        return fallback;
    }
    const size_t scopeStart = panelPos + panelNeedle.size();
    const size_t scopeEnd = payload.find("}", scopeStart);
    if (scopeEnd == std::string::npos) {
        return fallback;
    }
    const std::string scope = payload.substr(scopeStart, scopeEnd - scopeStart);
    const std::string needle = "\"" + key + "\":";
    const size_t keyPos = scope.find(needle);
    if (keyPos == std::string::npos) {
        return fallback;
    }
    size_t valueStart = keyPos + needle.size();
    if (valueStart < scope.size() && scope[valueStart] == '"') {
        const size_t valueEnd = scope.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) {
            return fallback;
        }
        return scope.substr(valueStart + 1, valueEnd - valueStart - 1);
    }
    size_t valueEnd = valueStart;
    while (valueEnd < scope.size() && scope[valueEnd] != ',' && scope[valueEnd] != '}') {
        ++valueEnd;
    }
    return scope.substr(valueStart, valueEnd - valueStart);
}

int ParseInt(const std::string& value, int fallback) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

void WorkspaceUiSettingsPersistence::LoadFromBytes(const std::vector<uint8_t>& bytes, WorkspaceUiState& uiState) {
    if (bytes.empty()) {
        return;
    }

    const std::string magic = Magic();
    auto it = std::search(bytes.begin(), bytes.end(), magic.begin(), magic.end());
    if (it == bytes.end()) {
        return;
    }
    const size_t payloadOffset = static_cast<size_t>(std::distance(bytes.begin(), it)) + magic.size();
    const std::string payload(payloadOffset < bytes.size()
                                  ? reinterpret_cast<const char*>(bytes.data() + payloadOffset)
                                  : "",
                              bytes.size() - std::min(payloadOffset, bytes.size()));

    auto contains = [&](const std::string& key, const std::string& value) {
        return payload.find("\"" + key + "\":" + value) != std::string::npos;
    };
    auto loadBool = [&](const std::string& key, bool fallback) {
        if (contains(key, "false")) return false;
        if (contains(key, "true")) return true;
        return fallback;
    };

    uiState.leftSidebarExpanded = !contains("leftSidebarExpanded", "false");
    uiState.rightSidebarExpanded = !contains("rightSidebarExpanded", "false");

    const WorkspacePanelId panelIds[] = {
        WorkspacePanelId::Color,
        WorkspacePanelId::BrushPreview,
        WorkspacePanelId::BrushControl,
        WorkspacePanelId::BrushPresets,
        WorkspacePanelId::Navigator,
        WorkspacePanelId::Layer,
        WorkspacePanelId::BrushSize,
        WorkspacePanelId::StatusBar
    };
    for (WorkspacePanelId panelId : panelIds) {
        if (auto* panel = uiState.FindPanel(panelId)) {
            const std::string key = PanelKey(panelId);
            const std::string side = PanelScopeValue(payload, key, "side");
            if (side == "left") panel->dockSide = WorkspaceDockSide::Left;
            else if (side == "right") panel->dockSide = WorkspaceDockSide::Right;
            else if (side == "floating") panel->dockSide = WorkspaceDockSide::Floating;

            panel->dockOrder = ParseInt(PanelScopeValue(payload, key, "order", std::to_string(panel->dockOrder)), panel->dockOrder);
            panel->dockedHeight = ParseInt(PanelScopeValue(payload, key, "dockH", std::to_string(panel->dockedHeight)), panel->dockedHeight);
            panel->visible = PanelScopeValue(payload, key, "visible", panel->visible ? "true" : "false") == "true";
            panel->collapsed = PanelScopeValue(payload, key, "collapsed", "false") == "true";
            panel->floatingRect.left = ParseInt(PanelScopeValue(payload, key, "x", std::to_string(panel->floatingRect.left)), panel->floatingRect.left);
            panel->floatingRect.top = ParseInt(PanelScopeValue(payload, key, "y", std::to_string(panel->floatingRect.top)), panel->floatingRect.top);
            const int width = ParseInt(PanelScopeValue(payload, key, "w", std::to_string(panel->floatingRect.Width())), panel->floatingRect.Width());
            const int height = ParseInt(PanelScopeValue(payload, key, "h", std::to_string(panel->floatingRect.Height())), panel->floatingRect.Height());
            panel->floatingRect.right = panel->floatingRect.left + width;
            panel->floatingRect.bottom = panel->floatingRect.top + height;
            panel->floatingZ = ParseInt(PanelScopeValue(payload, key, "z", std::to_string(panel->floatingZ)), panel->floatingZ);
        }
    }

    if (payload.find("\"panels\"") == std::string::npos) {
        uiState.SetPanelVisible(WorkspacePanelId::Color, loadBool("panelColor", uiState.IsPanelVisible(WorkspacePanelId::Color)));
        uiState.SetPanelVisible(WorkspacePanelId::BrushPreview, loadBool("panelBrushPreview", uiState.IsPanelVisible(WorkspacePanelId::BrushPreview)));
        uiState.SetPanelVisible(WorkspacePanelId::BrushControl, loadBool("panelBrushControl", uiState.IsPanelVisible(WorkspacePanelId::BrushControl)));
        uiState.SetPanelVisible(WorkspacePanelId::BrushPresets, loadBool("panelBrushPresets", uiState.IsPanelVisible(WorkspacePanelId::BrushPresets)));
        uiState.SetPanelVisible(WorkspacePanelId::Navigator, loadBool("panelNavigator", uiState.IsPanelVisible(WorkspacePanelId::Navigator)));
        uiState.SetPanelVisible(WorkspacePanelId::Layer, loadBool("panelLayer", uiState.IsPanelVisible(WorkspacePanelId::Layer)));
        uiState.SetPanelVisible(WorkspacePanelId::BrushSize, loadBool("panelBrushSize", uiState.IsPanelVisible(WorkspacePanelId::BrushSize)));
        uiState.SetPanelVisible(WorkspacePanelId::StatusBar, loadBool("panelStatusBar", uiState.IsPanelVisible(WorkspacePanelId::StatusBar)));
    }

    uiState.NormalizeAllDockOrders();
}

void WorkspaceUiSettingsPersistence::SaveToBytes(std::vector<uint8_t>& bytes, const WorkspaceUiState& uiState) {
    if (bytes.empty()) {
        bytes.resize(sizeof(uint32_t) * 2, 0);
    }

    const std::string magic = Magic();
    auto it = std::search(bytes.begin(), bytes.end(), magic.begin(), magic.end());
    if (it != bytes.end()) {
        bytes.erase(it, bytes.end());
    }

    std::ostringstream json;
    json << magic
         << "{\"workspace\":{\"leftSidebarExpanded\":"
         << (uiState.leftSidebarExpanded ? "true" : "false")
         << ",\"rightSidebarExpanded\":"
         << (uiState.rightSidebarExpanded ? "true" : "false")
         << ",\"panels\":{";

    bool first = true;
    for (const auto& panel : uiState.panels) {
        if (!first) {
            json << ",";
        }
        first = false;
        json << "\"" << PanelKey(panel.panelId) << "\":{\"side\":\""
             << (panel.dockSide == WorkspaceDockSide::Left ? "left" : panel.dockSide == WorkspaceDockSide::Right ? "right" : "floating")
             << "\",\"order\":" << panel.dockOrder
             << ",\"dockH\":" << panel.dockedHeight
             << ",\"visible\":" << (panel.visible ? "true" : "false")
             << ",\"collapsed\":" << (panel.collapsed ? "true" : "false")
             << ",\"x\":" << panel.floatingRect.left
             << ",\"y\":" << panel.floatingRect.top
             << ",\"w\":" << panel.floatingRect.Width()
             << ",\"h\":" << panel.floatingRect.Height()
             << ",\"z\":" << panel.floatingZ
             << "}";
    }
    json << "}}}";

    const std::string data = json.str();
    bytes.insert(bytes.end(), data.begin(), data.end());
}

} // namespace CloverPic::Core
