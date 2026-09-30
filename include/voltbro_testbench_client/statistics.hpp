#pragma once

#include <cstdint>

namespace voltbro::testbench {

struct VbdriveStatistics {
    uint64_t state_messages{};
    uint64_t command_messages_sent{};
    uint64_t malformed_messages{};
    double state_rate_hz{};
};

}  // namespace voltbro::testbench
