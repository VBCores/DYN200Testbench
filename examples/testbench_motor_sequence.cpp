#include "testbench_cli_common.hpp"

#include <iostream>
#include <thread>

using namespace voltbro::testbench;
using namespace testbench_cli;

namespace {

void usage() {
    std::cout
        << "usage: testbench_motor_sequence [--iface vcan1.0] [--node-id 11] [options]\n\n"
        << "Enable the motor and run three short stages: velocity, torque, then position.\n\n"
        << "Options:\n"
        << "  --iface NAME          SocketCAN interface to open. Default: vcan1.0\n"
        << "  --node-id N           Target motor node ID. Default: 11\n"
        << "  --stage-ms N          Duration of each stage. Default: 1500\n"
        << "  --velocity VALUE      Velocity stage setpoint, rad/s. Default: 0.5\n"
        << "  --torque VALUE        Torque stage setpoint, Nm. Default: 0.05\n"
        << "  --position-step VALUE Position stage offset from observed start position, rad. Default: 0.2\n"
        << "  --leave-enabled       Do not send state.is_on=false at the end.\n"
        << "  -h, --help            Show this help.\n\n"
        << "The tool sends setpoints at 50 Hz, stops between stages, and prints a final bus summary.\n\n";
    printSummaryFormat();
}

void runFor(CyphalInterfacePtr& bus,
            VbdriveClient& motor,
            uint8_t node_id,
            const std::string& mode,
            float value,
            std::chrono::milliseconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    auto next_tx = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_tx) {
            if (mode == "velocity") motor.setVelocity(node_id, value);
            else if (mode == "torque") motor.setTorque(node_id, value);
            else if (mode == "position") motor.setPosition(node_id, value);
            next_tx = now + std::chrono::milliseconds(20);
        }
        bus->loop();
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (wantsHelp(argc, argv)) {
            usage();
            return 0;
        }
        const auto iface = optionValue(argc, argv, "--iface", "vcan1.0");
        const auto node_id = static_cast<uint8_t>(optionInt(argc, argv, "--node-id", 11));
        const int stage_ms = optionInt(argc, argv, "--stage-ms", 1500);
        const float velocity = static_cast<float>(optionDouble(argc, argv, "--velocity", 0.5));
        const float torque = static_cast<float>(optionDouble(argc, argv, "--torque", 0.05));
        const float position_step = static_cast<float>(optionDouble(argc, argv, "--position-step", 0.2));
        const bool leave_enabled = hasFlag(argc, argv, "--leave-enabled");

        auto bus = makeCyphalInterface(iface, 100);
        VbdriveClient motor(bus);
        Dyn200Client dyn(bus);
        BusSnapshot snapshot;
        attachSnapshotCallbacks(snapshot, motor, dyn);

        auto enable_result = motor.setMotorEnabledAndWait(node_id, true, std::chrono::milliseconds(1000));
        if (!enable_result || !enable_result->bit_value || !*enable_result->bit_value) {
            std::cerr << "state.is_on was not confirmed by node " << unsigned(node_id) << "\n";
            return 1;
        }
        std::cout << "state.is_on confirmed true\n";

        const auto warmup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < warmup_deadline) {
            bus->loop();
        }
        const float start_pos = snapshot.last_motor ? snapshot.last_motor->position_rad.value_or(0.0F) : 0.0F;

        std::cout << "stage velocity value=" << velocity << "\n";
        runFor(bus, motor, node_id, "velocity", velocity, std::chrono::milliseconds(stage_ms));
        motor.stopMotor(node_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "stage torque value=" << torque << "\n";
        runFor(bus, motor, node_id, "torque", torque, std::chrono::milliseconds(stage_ms));
        motor.setTorque(node_id, 0.0F);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "stage position target=" << (start_pos + position_step) << "\n";
        runFor(bus, motor, node_id, "position", start_pos + position_step, std::chrono::milliseconds(stage_ms));
        motor.stopMotor(node_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!leave_enabled) {
            motor.disableMotor(node_id);
            std::cout << "state.is_on disable sent\n";
        }
        printSummary(snapshot, std::chrono::milliseconds(stage_ms * 3 + 500));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
