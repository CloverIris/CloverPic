#pragma once

#include <chrono>
#include <cstdint>

namespace CloverPic::Platform::Mac {

inline uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace CloverPic::Platform::Mac
