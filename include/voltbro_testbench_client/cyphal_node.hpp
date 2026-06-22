#pragma once

#include "errors.hpp"

#include <cstdlib>

#include <cyphal/allocators/sys/sys_allocator.h>
#include <cyphal/cyphal.h>
#include <cyphal/definitions.h>
#include <cyphal/providers/LinuxCAN.h>
#include <libcanard/canard.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace voltbro::testbench {

constexpr uint16_t kHeartbeatSubjectId = 7509;
constexpr uint16_t kDyn200StateSubjectId = 5100;
constexpr uint16_t kDyn200StatusSubjectId = 5101;
constexpr uint16_t kDyn200CommandSubjectId = 5102;
constexpr uint16_t kBrakeCommandSubjectId = 5103;
constexpr uint16_t kVbdriveStateSimpleSubjectId = 3811;
constexpr uint16_t kVbdriveCommandBaseSubjectId = 2107;
constexpr uint16_t kVbdriveSpecificControlBaseSubjectId = 3407;
constexpr uint16_t kRegisterAccessServiceId = 384;
constexpr size_t kDefaultTxQueueLength = 256;

using CyphalInterfacePtr = std::shared_ptr<CyphalInterface>;

struct CyphalTransfer {
    uint8_t priority{};
    uint16_t subject_id{};
    uint8_t source_node_id{};
    uint8_t destination_node_id{};
    bool request{};
    uint8_t transfer_id{};
    std::chrono::steady_clock::time_point host_receive_time{};
};

inline CyphalTransfer transferMetadata(const CanardRxTransfer* transfer,
                                       uint16_t port_id,
                                       bool request = false) {
    CyphalTransfer out;
    if (transfer != nullptr) {
        out.priority = static_cast<uint8_t>(transfer->metadata.priority);
        out.source_node_id = static_cast<uint8_t>(transfer->metadata.remote_node_id);
        out.transfer_id = transfer->metadata.transfer_id;
    }
    out.subject_id = port_id;
    out.destination_node_id = 0;
    out.request = request;
    out.host_receive_time = std::chrono::steady_clock::now();
    return out;
}

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
