#pragma once

#include "Core/Presentation/IconPainter.h"
#include "Core/Presentation/UiFrameGraph.h"
#include <cstdint>

namespace CloverPic::CoreUI {

enum class IconPlacement {
    None,
    Leading,
    Center,
    Trailing
};

enum class UiNodeType {
    Panel,
    Button,
    Canvas,
    ToolbarItem,
    LayerItem,
    Swatch,
    Slider,
    Text,
    SearchBox,
    RecentItem,
    ActionItem,
    Tile,
    MenuHeader,
    MenuItem,
    MenuSeparator,
    ColorField,
    HueStrip,
    BrushPresetItem,
    BrushSizeChip,
    CheckBox
};

struct UiNode {
    Presentation::UiNodeId id = 0;
    Presentation::UiNodeId parentId = 0;
    UiNodeType type = UiNodeType::Panel;
    Rect bounds;
    String label;
    String accessibilityLabel;
    String tooltip;
    Presentation::IconId iconId = Presentation::IconId::None;
    IconPlacement iconPlacement = IconPlacement::None;
    uint32_t command = 0;
    uint64_t userData = 0;
    String payload;
    String shortcut;
    std::vector<Presentation::UiNodeId> children;
    uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive;
    int zOrder = 0;
    Presentation::RefreshClass refreshClass = Presentation::RefreshClass::Static;
    bool checked = false;
    bool disabled = false;
    bool hasSubmenu = false;
};

class UiScene {
public:
    void Clear();
    Presentation::UiNodeId AddNode(UiNode node);
    UiNode* Find(Presentation::UiNodeId id);
    const UiNode* Find(Presentation::UiNodeId id) const;
    UiNode* HitTest(const Point& point);
    const std::vector<UiNode>& Nodes() const { return m_nodes; }

    void SetHover(Presentation::UiNodeId id) { m_hoverNode = id; }
    Presentation::UiNodeId GetHover() const { return m_hoverNode; }
    void SetCapture(Presentation::UiNodeId id) { m_captureNode = id; }
    Presentation::UiNodeId GetCapture() const { return m_captureNode; }
    void SetFocus(Presentation::UiNodeId id) { m_focusNode = id; }
    Presentation::UiNodeId GetFocus() const { return m_focusNode; }

    void PushModal(Presentation::UiNodeId id);
    void PopModal();
    void ClearModals();
    bool HasModal() const { return !m_modalStack.empty(); }

private:
    Rect ResolveAbsoluteBounds(Presentation::UiNodeId id) const;
    UiNode* HitTestNode(Presentation::UiNodeId id, const Point& point);

    std::vector<UiNode> m_nodes;
    Presentation::UiNodeId m_nextId = 1;
    Presentation::UiNodeId m_hoverNode = 0;
    Presentation::UiNodeId m_captureNode = 0;
    Presentation::UiNodeId m_focusNode = 0;
    std::vector<Presentation::UiNodeId> m_modalStack;
};

} // namespace CloverPic::CoreUI
