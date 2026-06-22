#pragma once

#include <cstdint>

namespace voltbro::testbench {

struct InterfaceStatistics {
    uint64_t frames_rx{};
    uint64_t frames_tx{};
    uint64_t rx_errors{};
    uint64_t tx_errors{};
    uint64_t dropped_frames{};
};

struct CyphalStatistics {
    uint64_t transfers_rx{};
    uint64_t transfers_tx{};
    uint64_t parse_errors{};
    uint64_t serialization_errors{};
    uint64_t unknown_subjects{};
    uint64_t heartbeat_rx{};
};

struct Dyn200Statistics {
    uint64_t state_messages{};
    uint64_t status_messages{};
    uint64_t command_messages_sent{};
    uint64_t malformed_messages{};
    double state_rate_hz{};
    double status_rate_hz{};
};

struct VbdriveStatistics {
    uint64_t state_messages{};
    uint64_t command_messages_sent{};
    uint64_t malformed_messages{};
    double state_rate_hz{};
};

struct ClientStatistics {
    InterfaceStatistics motor_interface;
    InterfaceStatistics dyn_interface;
    CyphalStatistics motor_cyphal;
    CyphalStatistics dyn_cyphal;
    Dyn200Statistics dyn200;
    VbdriveStatistics vbdrive;
};

}  // namespace voltbro::testbench
