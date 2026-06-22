#include "testbench_cli_common.hpp"

#include <iostream>

using namespace voltbro::testbench;
using namespace testbench_cli;

namespace {

void usage() {
    std::cout
        << "usage: testbench_discover [--iface vcan1.0] [--duration-ms 3000]\n\n"
        << "List devices visible on one shared testbench Cyphal/CAN bus and validate decoded protocols.\n\n"
        << "Options:\n"
        << "  --iface NAME          SocketCAN interface to open. Default: vcan1.0\n"
        << "  --duration-ms N       Observation window in milliseconds. Default: 3000\n"
        << "  -h, --help            Show this help.\n\n";
    printKnownPorts();
    printSummaryFormat();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (wantsHelp(argc, argv)) {
            usage();
            return 0;
        }
        installSignalHandlers();
        const auto iface = optionValue(argc, argv, "--iface", "vcan1.0");
        const int duration_ms = optionInt(argc, argv, "--duration-ms", 3000);

        auto bus = makeCyphalInterface(iface, 100);
        BusSnapshot snapshot;
        HeartbeatSubscription heartbeat(bus, snapshot);
        VbdriveClient motor(bus);
        Dyn200Client dyn(bus);
        attachSnapshotCallbacks(snapshot, motor, dyn);

        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::milliseconds(duration_ms);
        while (!g_stop && std::chrono::steady_clock::now() < deadline) {
            bus->loop();
        }

        std::cout << "Interface: " << iface << "\n";
        printKnownPorts();
        printSummary(snapshot, std::chrono::steady_clock::now() - start);

        const bool motor_ok = snapshot.vbdrive_state > 0;
        const bool dyn_ok = snapshot.dyn_state > 0 || snapshot.dyn_status > 0;
        std::cout << "Protocol check: motor=" << (motor_ok ? "ok" : "not seen")
                  << " dyn200=" << (dyn_ok ? "ok" : "not seen") << "\n";
        return (motor_ok && dyn_ok) ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
