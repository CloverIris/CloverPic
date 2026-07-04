#pragma once

#include "Utils/Types.h"
#include <cstdint>
#include <vector>

namespace CloverPic::Presentation {

using UiNodeId = uint64_t;

enum UiNodeFlags : uint32_t {
    UiNodeVisible = 1u << 0,
    UiNodeInteractive = 1u << 1,
    UiNodeAnimated = 1u << 2,
    UiNodeRoot = 1u << 3
};

enum class RefreshClass {
    Static,
    Interactive,
    Animation30Hz,
    Animation60Hz
};

struct UiLayoutNode {
    UiNodeId id = 0;
    UiNodeId parentId = 0;
    Rect bounds;
    String debugName;
    uint32_t flags = UiNodeVisible | UiNodeInteractive;
    int zOrder = 0;
    RefreshClass refreshClass = RefreshClass::Static;

    bool IsVisible() const { return (flags & UiNodeVisible) != 0; }
    bool IsInteractive() const { return (flags & UiNodeInteractive) != 0; }
};

struct DirtyRegion {
    UiNodeId nodeId = 0;
    Rect rect;
    RefreshClass refreshClass = RefreshClass::Static;
};

struct AnimatedRegion {
    UiNodeId nodeId = 0;
    Rect rect;
    uint32_t targetFps = 60;
    uint64_t lastFrameMs = 0;
};

class UiFrameGraph {
public:
    static UiFrameGraph& Get();

    UiNodeId AllocateNodeId();
    void UpsertNode(const UiLayoutNode& node);
    void RemoveNode(UiNodeId id);
    const UiLayoutNode* FindNode(UiNodeId id) const;

    UiNodeId HitTest(UiNodeId rootId, const Point& point) const;

    void InvalidateNode(UiNodeId nodeId);
    void InvalidateRect(UiNodeId nodeId, const Rect& rect);
    std::vector<DirtyRegion> ConsumeDirtyRegions();

    void SetAnimatedRegion(UiNodeId nodeId, const Rect& rect, uint32_t targetFps);
    void ClearAnimatedRegion(UiNodeId nodeId);
    std::vector<DirtyRegion> CollectDueAnimatedRegions(uint64_t nowMs);

private:
    UiFrameGraph() = default;

    Rect ResolveAbsoluteBounds(UiNodeId nodeId) const;
    UiNodeId HitTestRecursive(UiNodeId nodeId, const Point& point) const;
    RefreshClass RefreshClassForNode(UiNodeId nodeId) const;

    std::vector<UiLayoutNode> m_nodes;
    std::vector<DirtyRegion> m_dirtyRegions;
    std::vector<AnimatedRegion> m_animatedRegions;
    UiNodeId m_nextNodeId = 1;
};

} // namespace CloverPic::Presentation
