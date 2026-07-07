#pragma once

#include "Utils/Types.h"
#include <cstdint>

namespace CloverPic {

struct MemoryAdvice {
    size_t maxCanvasWidth = 0;
    size_t maxCanvasHeight = 0;
    size_t maxLayers = 0;
    size_t memoryPerLayer = 0;
    size_t totalEstimatedUsage = 0;
    uint64_t usableBudget = 0;
    uint64_t totalPhysical = 0;
};

enum class MemoryStatus {
    Safe,     // Green: < 50% of budget
    Warning,  // Yellow: 50-80% of budget
    Danger    // Red: > 80% of budget (or exceeds)
};

class MemoryAdvisor {
public:
    MemoryAdvisor() = default;
    
    // Calculate memory advice based on current system state
    MemoryAdvice CalculateAdvice(uint32_t canvasWidth, uint32_t canvasHeight, 
                                  uint32_t layerCount);
    
    // Get raw system memory info
    uint64_t GetTotalPhysicalMemory() const;
    uint64_t GetAvailablePhysicalMemory() const;
    
    // Check if operation is safe
    MemoryStatus CheckStatus(size_t estimatedUsage) const;
    
    // Format bytes to human readable string
    static String FormatBytes(uint64_t bytes);
    
private:
    static constexpr double SAFETY_FACTOR = 0.80;
    static constexpr uint64_t SYSTEM_OVERHEAD = 512ULL * 1024 * 1024; // 512 MB
    static constexpr double LAYER_OVERHEAD_FACTOR = 1.20; // 20% extra for thumbnails/masks
};

} // namespace CloverPic
