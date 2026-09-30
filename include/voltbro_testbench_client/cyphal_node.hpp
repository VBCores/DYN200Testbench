#pragma once

#include "errors.hpp"

#include <cstdlib>

#include <cyphal/allocators/sys/sys_allocator.h>
#include <cyphal/cyphal.h>
#include <cyphal/definitions.h>
#include <cyphal/providers/LinuxCAN.h>
#include <libcanard/canard.h>

#include <cstdint>
#include <memory>
#include <string>

namespace voltbro::testbench {

constexpr uint16_t kHeartbeatSubjectId = 7509;
constexpr uint8_t kDyn200NodeId = 79;
constexpr uint16_t kDyn200SpeedSubjectId = 5100;
constexpr uint16_t kDyn200TorqueSubjectId = 5101;
constexpr uint16_t kBrakeCommandSubjectId = 5103;
constexpr uint16_t kVbdriveStateSubjectId = 3811;
constexpr uint16_t kVbdriveMitBaseSubjectId = 2107;
constexpr uint16_t kVbdriveServoBaseSubjectId = 3407;
constexpr uint16_t kRegisterAccessServiceId = 384;
constexpr size_t kDefaultTxQueueLength = 256;

using CyphalInterfacePtr = std::shared_ptr<CyphalInterface>;

inline CyphalInterfacePtr makeCyphalInterface(const std::string& ifname,
                                              uint8_t local_node_id,
                                              size_t tx_queue_len = kDefaultTxQueueLength) {
    if (local_node_id > CANARD_NODE_ID_MAX) {
        throw ProtocolError("local Cyphal node ID must be in range 0..127");
    }
    return CyphalInterface::create_heap<LinuxCAN, SystemAllocator>(
        local_node_id,
        ifname,
        tx_queue_len,
        DEFAULT_CONFIG);
}

inline void flushCyphalTx(const CyphalInterfacePtr& interface) {
    if (!interface) {
        throw ProtocolError("Cyphal interface is not initialized");
    }
    while (interface->has_unsent_frames()) {
        interface->process_tx_once();
    }
}

}  // namespace voltbro::testbench
