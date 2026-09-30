#include <cyphal/allocators/sys/sys_allocator.h>
#include <cyphal/cyphal.h>
#include <cyphal/providers/LinuxCAN.h>
#include <uavcan/primitive/scalar/Real32_1_0.hpp>

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {

constexpr CanardNodeID kLocalNodeId = 101;
constexpr CanardPortID kBrakeSubjectId = 5103;
constexpr const char* kCanInterface = "vcan2.0";
using Real32 = uavcan_primitive_scalar_Real32_1_0;

volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) {
    stop_requested = 1;
}

void send_brake(CyphalInterface& bus, CanardTransferID& transfer_id, float value) {
    Real32 command{};
    command.value = value;
    bus.send_msg(&command, kBrakeSubjectId, &transfer_id);
    while (bus.has_unsent_frames()) {
        bus.process_tx_once();
    }
    std::cout << "5103 brake: " << value << '\n';
}

}  // namespace

int main() {
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    auto bus = CyphalInterface::create_heap<LinuxCAN, SystemAllocator>(
        kLocalNodeId, kCanInterface, 256, DEFAULT_CONFIG);

    CanardTransferID transfer_id = 0;
    bool send_half = false;
    auto next_send = std::chrono::steady_clock::now();

    std::cout << "Sending brake command on " << kCanInterface
              << "; Ctrl+C sends 0 and exits\n";

    while (!stop_requested) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_send) {
            send_brake(*bus, transfer_id, send_half ? 0.5F : 0.0F);
            send_half = !send_half;
            next_send += std::chrono::seconds(1);
        } else {
            std::this_thread::sleep_until(next_send);
        }
    }

    send_brake(*bus, transfer_id, 0.0F);
    return 0;
}
