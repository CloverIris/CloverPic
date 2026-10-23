#include "Core/UI/Workspace/Clover/WorkspaceActionRouter.h"

#include "Core/App/WorkspaceRuntime.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace CloverPic::Core {
namespace {

std::vector<std::string> Split(const std::string& value, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (const char ch : value) {
        if (ch == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

bool ParseNumber(const std::string& text, int& out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str()) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

} // namespace

bool WorkspaceActionRouter::Handle(const std::string& action) {
    if (!m_runtime) {
        return false;
    }
    const std::vector<std::string> parts = Split(action, ':');
    if (parts.empty()) {
        return false;
    }

    // change:<control>:<value>
    if (parts.size() >= 3 && parts[0] == "change") {
        int value = 0;
        if (!ParseNumber(parts[2], value)) {
            return false;
        }
        const std::string& control = parts[1];
        if (control == "size") {
            m_runtime->InvokeBrushParam(BrushParamId::Size, static_cast<uint16_t>(value));
            return true;
        }
        if (control == "opacity") {
            m_runtime->InvokeBrushParam(BrushParamId::Opacity, static_cast<uint16_t>(value));
            return true;
        }
        if (control == "flow") {
            m_runtime->InvokeBrushParam(BrushParamId::Flow, static_cast<uint16_t>(value));
            return true;
        }
        if (control == "spacing") {
            m_runtime->InvokeBrushParam(BrushParamId::Spacing, static_cast<uint16_t>(value));
            return true;
        }
        if (control == "blend") {
            m_runtime->InvokeSetBlendMode(static_cast<uint32_t>(value));
            return true;
        }
        if (control == "layerOpacity") {
            m_runtime->InvokeSetLayerOpacity(static_cast<uint8_t>(value));
            return true;
        }
        return false;
    }

    // tool:<name> or layer:select:<index> / layer:visible:<index>
    if (parts[0] == "tool" && parts.size() >= 2) {
        const std::string& tool = parts[1];
        if (tool == "brush") m_runtime->InvokeSelectTool(ToolType::Brush);
        else if (tool == "eraser") m_runtime->InvokeSelectTool(ToolType::Eraser);
        else if (tool == "fill") m_runtime->InvokeSelectTool(ToolType::Fill);
        else if (tool == "move") m_runtime->InvokeSelectTool(ToolType::Move);
        else if (tool == "select") m_runtime->InvokeSelectTool(ToolType::RectSelect);
        else if (tool == "eyedropper") m_runtime->InvokeSelectTool(ToolType::Eyedropper);
        else return false;
        return true;
    }
    if (parts[0] == "layer" && parts.size() >= 3 && parts[1] == "select") {
        int index = 0;
        if (!ParseNumber(parts[2], index)) {
            return false;
        }
        m_runtime->InvokeSelectLayer(static_cast<size_t>(index));
        return true;
    }
    // Context-menu rows: layer:0 duplicate, 1 delete, 2 submenu, 3 toggle visible.
    if (parts[0] == "layer" && parts.size() >= 2 && parts[1] != "select") {
        int index = 0;
        if (!ParseNumber(parts[1], index)) {
            return false;
        }
        switch (index) {
        case 0: m_runtime->InvokeDuplicateLayer(); return true;
        case 1: m_runtime->InvokeDeleteLayer(); return true;
        case 2: return false; // the host cascades into a submenu
        case 3: m_runtime->InvokeToggleActiveLayerVisibility(); return true;
        default: return false;
        }
    }
    // Submenu rows: merge:0 merge down.
    if (parts[0] == "merge" && parts.size() >= 2) {
        int index = 0;
        if (!ParseNumber(parts[1], index)) {
            return false;
        }
        if (index == 0) {
            m_runtime->InvokeMergeLayerDown();
            return true;
        }
        return false;
    }
    if (parts[0] == "zoom" && parts.size() >= 2) {
        int percent = 0;
        if (!ParseNumber(parts[1], percent)) {
            return false;
        }
        m_runtime->InvokeZoom(percent);
        return true;
    }
    return false;
}

} // namespace CloverPic::Core
