#include "Core/Presentation/FrameScheduler.h"
#include <algorithm>

namespace CloverPic::Presentation {

void FrameScheduler::Invalidate(const Rect& rect, RefreshClass) {
    if (IsEmpty(rect)) {
        return;
    }
    MergeIntoDirtyList(rect);
}

void FrameScheduler::SetAnimatedRegion(UiNodeId id, const Rect& rect, uint32_t targetFps) {
    if (id == 0 || IsEmpty(rect) || targetFps == 0) {
        return;
    }

    auto it = std::find_if(m_animatedRegions.begin(), m_animatedRegions.end(), [&](const AnimatedRegionState& region) {
        return region.id == id;
    });
    if (it == m_animatedRegions.end()) {
        m_animatedRegions.push_back(AnimatedRegionState{ id, rect, targetFps, 0 });
    } else {
        it->rect = rect;
        it->targetFps = targetFps;
    }
}

void FrameScheduler::ClearAnimatedRegion(UiNodeId id) {
    m_animatedRegions.erase(std::remove_if(m_animatedRegions.begin(), m_animatedRegions.end(),
        [&](const AnimatedRegionState& region) { return region.id == id; }), m_animatedRegions.end());
}

bool FrameScheduler::HasPendingFrame(uint64_t nowMs) const {
    if (!m_dirtyRects.empty()) {
        return true;
    }
    return std::any_of(m_animatedRegions.begin(), m_animatedRegions.end(), [&](const AnimatedRegionState& region) {
        return AnimatedRegionDue(region, nowMs);
    });
}

std::vector<Rect> FrameScheduler::ConsumeDirtyRects(uint64_t nowMs, const Rect& frameBounds) {
    for (auto& region : m_animatedRegions) {
        if (AnimatedRegionDue(region, nowMs)) {
            region.lastFrameMs = nowMs;
            MergeIntoDirtyList(region.rect);
        }
    }

    std::vector<Rect> clipped;
    clipped.reserve(m_dirtyRects.size());
    for (const auto& rect : m_dirtyRects) {
        Rect clamped = ClampRect(rect, frameBounds);
        if (!IsEmpty(clamped)) {
            clipped.push_back(clamped);
        }
    }
    m_dirtyRects.clear();
    return clipped;
}

void FrameScheduler::Reset() {
    m_dirtyRects.clear();
    m_animatedRegions.clear();
}

Rect FrameScheduler::ClampRect(const Rect& rect, const Rect& bounds) {
    return Rect(
        std::max(rect.left, bounds.left),
        std::max(rect.top, bounds.top),
        std::min(rect.right, bounds.right),
        std::min(rect.bottom, bounds.bottom));
}

bool FrameScheduler::IsEmpty(const Rect& rect) {
    return rect.Width() <= 0 || rect.Height() <= 0;
}

bool FrameScheduler::CanMerge(const Rect& a, const Rect& b) {
    const Rect merged = MergeRect(a, b);
    const int64_t areaA = static_cast<int64_t>(a.Width()) * a.Height();
    const int64_t areaB = static_cast<int64_t>(b.Width()) * b.Height();
    const int64_t areaMerged = static_cast<int64_t>(merged.Width()) * merged.Height();
    return areaMerged <= (areaA + areaB) * 2;
}

Rect FrameScheduler::MergeRect(const Rect& a, const Rect& b) {
    return Rect(
        std::min(a.left, b.left),
        std::min(a.top, b.top),
        std::max(a.right, b.right),
        std::max(a.bottom, b.bottom));
}

uint64_t FrameScheduler::FrameIntervalMs(uint32_t targetFps) {
    return 1000 / std::max<uint32_t>(1, targetFps);
}

bool FrameScheduler::AnimatedRegionDue(const AnimatedRegionState& region, uint64_t nowMs) const {
    return region.lastFrameMs == 0 || nowMs - region.lastFrameMs >= FrameIntervalMs(region.targetFps);
}

void FrameScheduler::MergeIntoDirtyList(const Rect& rect) {
    for (auto& existing : m_dirtyRects) {
        if (CanMerge(existing, rect)) {
            existing = MergeRect(existing, rect);
            return;
        }
    }
    m_dirtyRects.push_back(rect);
}

} // namespace CloverPic::Presentation
