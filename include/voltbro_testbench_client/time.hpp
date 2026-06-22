#pragma once

#include <chrono>
#include <cstdint>

namespace voltbro::testbench {

inline uint64_t steadyTimeNs(std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now()) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count());
}

}  // namespace voltbro::testbench
