#include "Core/MemoryAdvisor.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace VividPic {

MemoryAdvisor& MemoryAdvisor::GetInstance() {
    static MemoryAdvisor instance;
    return instance;
}

uint64_t MemoryAdvisor::GetTotalPhysicalMemory() const {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return memStatus.ullTotalPhys;
    }
    return 0;
}

uint64_t MemoryAdvisor::GetAvailablePhysicalMemory() const {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return memStatus.ullAvailPhys;
    }
    return 0;
}

MemoryAdvice MemoryAdvisor::CalculateAdvice(uint32_t canvasWidth, uint32_t canvasHeight, 
                                             uint32_t layerCount) {
    MemoryAdvice advice;
    advice.totalPhysical = GetTotalPhysicalMemory();
    
    uint64_t memoryBudget = static_cast<uint64_t>(advice.totalPhysical * SAFETY_FACTOR);
    advice.usableBudget = (memoryBudget > SYSTEM_OVERHEAD) ? (memoryBudget - SYSTEM_OVERHEAD) : 0;
    
    // Single layer memory (RGBA32)
    advice.memoryPerLayer = static_cast<size_t>(canvasWidth) * canvasHeight * 4;
    advice.memoryPerLayer = static_cast<size_t>(advice.memoryPerLayer * LAYER_OVERHEAD_FACTOR);
    
    advice.totalEstimatedUsage = advice.memoryPerLayer * layerCount;
    
    // Calculate max layers for this canvas size
    if (advice.memoryPerLayer > 0) {
        advice.maxLayers = advice.usableBudget / advice.memoryPerLayer;
    }
    
    // Calculate max canvas dimensions (assuming square, 10 layers)
    uint64_t memPer10Layers = advice.usableBudget / 10;
    uint64_t pixelsPerLayer = memPer10Layers / static_cast<uint64_t>(4 * LAYER_OVERHEAD_FACTOR);
    uint64_t maxDimension = static_cast<uint64_t>(sqrt(static_cast<double>(pixelsPerLayer)));
    advice.maxCanvasWidth = static_cast<size_t>(maxDimension);
    advice.maxCanvasHeight = advice.maxCanvasWidth;
    
    return advice;
}

MemoryStatus MemoryAdvisor::CheckStatus(size_t estimatedUsage) const {
    uint64_t totalPhysical = GetTotalPhysicalMemory();
    uint64_t memoryBudget = static_cast<uint64_t>(totalPhysical * SAFETY_FACTOR);
    uint64_t usableBudget = (memoryBudget > SYSTEM_OVERHEAD) ? (memoryBudget - SYSTEM_OVERHEAD) : 0;
    
    if (usableBudget == 0) return MemoryStatus::Danger;
    
    double ratio = static_cast<double>(estimatedUsage) / static_cast<double>(usableBudget);
    
    if (ratio < 0.5) return MemoryStatus::Safe;
    if (ratio < 0.8) return MemoryStatus::Warning;
    return MemoryStatus::Danger;
}

String MemoryAdvisor::FormatBytes(uint64_t bytes) {
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(1);
    
    if (bytes >= 1024ULL * 1024 * 1024 * 1024) {
        oss << (bytes / (1024.0 * 1024 * 1024 * 1024)) << L" TB";
    } else if (bytes >= 1024ULL * 1024 * 1024) {
        oss << (bytes / (1024.0 * 1024 * 1024)) << L" GB";
    } else if (bytes >= 1024ULL * 1024) {
        oss << (bytes / (1024.0 * 1024)) << L" MB";
    } else if (bytes >= 1024ULL) {
        oss << (bytes / 1024.0) << L" KB";
    } else {
        oss << bytes << L" B";
    }
    
    return oss.str();
}

} // namespace VividPic
