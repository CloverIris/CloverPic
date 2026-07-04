#pragma once

#include "Core/Presentation/UiFrameGraph.h"
#include "Utils/Types.h"
#include <cstdint>
#include <vector>

namespace CloverPic::Presentation {

class FrameScheduler {
public:
    void Invalidate(const Rect& rect, RefreshClass refreshClass = RefreshClass::Static);
    void SetAnimatedRegion(UiNodeId id, const Rect& rect, uint32_t targetFps);
    void ClearAnimatedRegion(UiNodeId id);

    bool HasPendingFrame(uint64_t nowMs) const;
    std::vector<Rect> ConsumeDirtyRects(uint64_t nowMs, const Rect& frameBounds);
    void Reset();

private:
    struct AnimatedRegionState {
        UiNodeId id = 0;
        Rect rect;
        uint32_t targetFps = 60;
        uint64_t lastFrameMs = 0;
    };

    static Rect ClampRect(const Rect& rect, const Rect& bounds);
    static bool IsEmpty(const Rect& rect);
    static bool CanMerge(const Rect& a, const Rect& b);
    static Rect MergeRect(const Rect& a, const Rect& b);
    static uint64_t FrameIntervalMs(uint32_t targetFps);

    bool AnimatedRegionDue(const AnimatedRegionState& region, uint64_t nowMs) const;
    void MergeIntoDirtyList(const Rect& rect);

    std::vector<Rect> m_dirtyRects;
    std::vector<AnimatedRegionState> m_animatedRegions;
};

} // namespace CloverPic::Presentation
