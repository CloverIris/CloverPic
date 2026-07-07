#include "Core/UI/Scene/UiScene.h"
#include <algorithm>

namespace CloverPic::CoreUI {

void UiScene::Clear() {
    m_nodes.clear();
    m_nextId = 1;
    m_hoverNode = 0;
    m_captureNode = 0;
    m_focusNode = 0;
    m_modalStack.clear();
}

Presentation::UiNodeId UiScene::AddNode(UiNode node) {
    if (node.id == 0) {
        node.id = m_nextId++;
    }
    const Presentation::UiNodeId id = node.id;
    const Presentation::UiNodeId parentId = node.parentId;
    m_nodes.push_back(std::move(node));
    if (parentId != 0) {
        if (auto* parent = Find(parentId)) {
            parent->children.push_back(id);
        }
    }
    return id;
}

UiNode* UiScene::Find(Presentation::UiNodeId id) {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const UiNode& node) { return node.id == id; });
    return it == m_nodes.end() ? nullptr : &*it;
}

const UiNode* UiScene::Find(Presentation::UiNodeId id) const {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const UiNode& node) { return node.id == id; });
    return it == m_nodes.end() ? nullptr : &*it;
}

UiNode* UiScene::HitTest(const Point& point) {
    if (!m_modalStack.empty()) {
        return HitTestNode(m_modalStack.back(), point);
    }

    UiNode* best = nullptr;
    for (auto& node : m_nodes) {
        if (node.parentId != 0) {
            continue;
        }
        const bool visible = (node.flags & Presentation::UiNodeVisible) != 0;
        const bool interactive = (node.flags & Presentation::UiNodeInteractive) != 0;
        if (!visible || !interactive || !ResolveAbsoluteBounds(node.id).Contains(point)) {
            continue;
        }
        if (!best || node.zOrder >= best->zOrder) {
            best = &node;
        }
    }
    return best;
}

void UiScene::PushModal(Presentation::UiNodeId id) {
    if (id != 0) {
        m_modalStack.push_back(id);
    }
}

void UiScene::PopModal() {
    if (!m_modalStack.empty()) {
        m_modalStack.pop_back();
    }
}

void UiScene::ClearModals() {
    m_modalStack.clear();
}

Rect UiScene::ResolveAbsoluteBounds(Presentation::UiNodeId id) const {
    const auto* node = Find(id);
    if (!node) return Rect();
    Rect bounds = node->bounds;
    Presentation::UiNodeId parentId = node->parentId;
    while (const auto* parent = Find(parentId)) {
        bounds.left += parent->bounds.left;
        bounds.right += parent->bounds.left;
        bounds.top += parent->bounds.top;
        bounds.bottom += parent->bounds.top;
        parentId = parent->parentId;
    }
    return bounds;
}

UiNode* UiScene::HitTestNode(Presentation::UiNodeId id, const Point& point) {
    auto* node = Find(id);
    if (!node || (node->flags & Presentation::UiNodeVisible) == 0) {
        return nullptr;
    }
    if (!ResolveAbsoluteBounds(id).Contains(point)) {
        return nullptr;
    }

    UiNode* bestChild = nullptr;
    for (auto childId : node->children) {
        UiNode* hit = HitTestNode(childId, point);
        if (!hit) continue;
        if (!bestChild || hit->zOrder >= bestChild->zOrder) {
            bestChild = hit;
        }
    }
    if (bestChild) return bestChild;

    const bool interactive = (node->flags & Presentation::UiNodeInteractive) != 0;
    return interactive ? node : nullptr;
}

} // namespace CloverPic::CoreUI
