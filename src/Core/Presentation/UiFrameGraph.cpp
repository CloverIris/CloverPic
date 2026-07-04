#include "Core/Presentation/UiFrameGraph.h"
#include <algorithm>

namespace CloverPic::Presentation {

UiFrameGraph& UiFrameGraph::Get() {
    static UiFrameGraph graph;
    return graph;
}

UiNodeId UiFrameGraph::AllocateNodeId() {
    return m_nextNodeId++;
}

void UiFrameGraph::UpsertNode(const UiLayoutNode& node) {
    if (node.id == 0) {
        return;
    }

    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const UiLayoutNode& item) {
        return item.id == node.id;
    });
    if (it == m_nodes.end()) {
        m_nodes.push_back(node);
    } else {
        *it = node;
    }
}

void UiFrameGraph::RemoveNode(UiNodeId id) {
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(), [&](const UiLayoutNode& node) {
        return node.id == id || node.parentId == id;
    }), m_nodes.end());

    m_dirtyRegions.erase(std::remove_if(m_dirtyRegions.begin(), m_dirtyRegions.end(), [&](const DirtyRegion& region) {
        return region.nodeId == id;
    }), m_dirtyRegions.end());

    ClearAnimatedRegion(id);
}

const UiLayoutNode* UiFrameGraph::FindNode(UiNodeId id) const {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const UiLayoutNode& node) {
        return node.id == id;
    });
    return it == m_nodes.end() ? nullptr : &*it;
}

UiNodeId UiFrameGraph::HitTest(UiNodeId rootId, const Point& point) const {
    return HitTestRecursive(rootId, point);
}

void UiFrameGraph::InvalidateNode(UiNodeId nodeId) {
    if (const auto* node = FindNode(nodeId)) {
        InvalidateRect(nodeId, Rect(0, 0, node->bounds.Width(), node->bounds.Height()));
    }
}

void UiFrameGraph::InvalidateRect(UiNodeId nodeId, const Rect& rect) {
    if (nodeId == 0 || rect.Width() <= 0 || rect.Height() <= 0) {
        return;
    }
    m_dirtyRegions.push_back(DirtyRegion{ nodeId, rect, RefreshClassForNode(nodeId) });
}

std::vector<DirtyRegion> UiFrameGraph::ConsumeDirtyRegions() {
    auto regions = std::move(m_dirtyRegions);
    m_dirtyRegions.clear();
    return regions;
}

void UiFrameGraph::SetAnimatedRegion(UiNodeId nodeId, const Rect& rect, uint32_t targetFps) {
    if (nodeId == 0 || targetFps == 0) {
        return;
    }

    auto it = std::find_if(m_animatedRegions.begin(), m_animatedRegions.end(), [&](const AnimatedRegion& region) {
        return region.nodeId == nodeId;
    });
    AnimatedRegion region{ nodeId, rect, targetFps, 0 };
    if (it == m_animatedRegions.end()) {
        m_animatedRegions.push_back(region);
    } else {
        *it = region;
    }
}

void UiFrameGraph::ClearAnimatedRegion(UiNodeId nodeId) {
    m_animatedRegions.erase(std::remove_if(m_animatedRegions.begin(), m_animatedRegions.end(), [&](const AnimatedRegion& region) {
        return region.nodeId == nodeId;
    }), m_animatedRegions.end());
}

std::vector<DirtyRegion> UiFrameGraph::CollectDueAnimatedRegions(uint64_t nowMs) {
    std::vector<DirtyRegion> due;
    for (auto& region : m_animatedRegions) {
        const uint64_t frameMs = 1000 / std::max<uint32_t>(1, region.targetFps);
        if (region.lastFrameMs == 0 || nowMs - region.lastFrameMs >= frameMs) {
            region.lastFrameMs = nowMs;
            due.push_back(DirtyRegion{ region.nodeId, region.rect, RefreshClassForNode(region.nodeId) });
        }
    }
    return due;
}

Rect UiFrameGraph::ResolveAbsoluteBounds(UiNodeId nodeId) const {
    const auto* node = FindNode(nodeId);
    if (!node) {
        return Rect();
    }

    Rect resolved = node->bounds;
    UiNodeId parentId = node->parentId;
    while (const auto* parent = FindNode(parentId)) {
        resolved.left += parent->bounds.left;
        resolved.right += parent->bounds.left;
        resolved.top += parent->bounds.top;
        resolved.bottom += parent->bounds.top;
        parentId = parent->parentId;
    }
    return resolved;
}

UiNodeId UiFrameGraph::HitTestRecursive(UiNodeId nodeId, const Point& point) const {
    const auto* node = FindNode(nodeId);
    if (!node || !node->IsVisible()) {
        return 0;
    }

    const Rect absolute = ResolveAbsoluteBounds(nodeId);
    if (!absolute.Contains(point)) {
        return 0;
    }

    UiNodeId bestChild = 0;
    int bestZ = 0;
    for (const auto& child : m_nodes) {
        if (child.parentId != nodeId) {
            continue;
        }
        UiNodeId hit = HitTestRecursive(child.id, point);
        if (hit != 0 && (!bestChild || child.zOrder >= bestZ)) {
            bestChild = hit;
            bestZ = child.zOrder;
        }
    }

    if (bestChild) {
        return bestChild;
    }
    return node->IsInteractive() ? node->id : 0;
}

RefreshClass UiFrameGraph::RefreshClassForNode(UiNodeId nodeId) const {
    if (const auto* node = FindNode(nodeId)) {
        return node->refreshClass;
    }
    return RefreshClass::Static;
}

} // namespace CloverPic::Presentation
