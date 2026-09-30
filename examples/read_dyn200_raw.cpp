#include <cyphal/allocators/sys/sys_allocator.h>
#include <cyphal/cyphal.h>
#include <cyphal/providers/LinuxCAN.h>
#include <uavcan/primitive/scalar/Real32_1_0.hpp>

#include <iostream>

namespace {

constexpr CanardNodeID kLocalNodeId = 101;
constexpr CanardNodeID kDyn200NodeId = 79;
constexpr CanardPortID kSpeedSubjectId = 5100;
constexpr CanardPortID kTorqueSubjectId = 5101;
constexpr const char* kCanInterface = "vcan2.0";
using Float32 = uavcan_primitive_scalar_Real32_1_0;

}  // namespace

int main() {
    auto bus = CyphalInterface::create_heap<LinuxCAN, SystemAllocator>(
        kLocalNodeId, kCanInterface, 256, DEFAULT_CONFIG);

    bus->subscribe<Float32>(kSpeedSubjectId, [&](const Float32& msg, CanardRxTransfer* transfer) {
        if (transfer == nullptr || transfer->metadata.remote_node_id != kDyn200NodeId) return;
        std::cout << "5100 speed: " << msg.value << " rpm\n";
    });

    bus->subscribe<Float32>(kTorqueSubjectId, [&](const Float32& msg, CanardRxTransfer* transfer) {
        if (transfer == nullptr || transfer->metadata.remote_node_id != kDyn200NodeId) return;
        std::cout << "5101 torque: " << msg.value << " N*m\n";
    });

    std::cout << "Listening for DYN-200 node " << static_cast<unsigned>(kDyn200NodeId)
              << " on " << kCanInterface << " (Ctrl+C to stop)\n";
    while (true) {
        bus->loop();
    }
}
