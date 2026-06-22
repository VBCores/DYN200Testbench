#include "testbench_cli_common.hpp"

#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

using namespace voltbro::testbench;
using namespace testbench_cli;

namespace {

std::string brakeTransferIdPath(const std::string& iface, uint8_t local_node_id) {
    std::string safe_iface;
    safe_iface.reserve(iface.size());
    for (const unsigned char ch : iface) {
        safe_iface += (std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-') ? static_cast<char>(ch) : '_';
    }
    return "/tmp/voltbro_testbench_brake_5103_" + safe_iface + "_node" +
           std::to_string(unsigned(local_node_id)) + ".tid";
}

CanardTransferID loadBrakeTransferId(const std::string& iface, uint8_t local_node_id) {
    std::ifstream in(brakeTransferIdPath(iface, local_node_id));
    unsigned value = 0;
    if (in >> value) {
        return static_cast<CanardTransferID>(value % 32U);
    }
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return static_cast<CanardTransferID>(static_cast<unsigned long long>(ticks) % 32ULL);
}

void saveBrakeTransferId(const std::string& iface, uint8_t local_node_id, CanardTransferID transfer_id) {
    std::ofstream out(brakeTransferIdPath(iface, local_node_id), std::ios::trunc);
    if (out) {
        out << unsigned(transfer_id % 32U) << "\n";
    }
}

void usage() {
    std::cout
        << "usage:\n"
        << "  testbench_command --iface vcan1.0 motor --node-id 11 enable|disable|stop\n"
        << "  testbench_command --iface vcan1.0 motor --node-id 11 velocity|torque|position|voltage VALUE\n"
        << "  testbench_command --iface vcan1.0 motor --node-id 11 foc --torque 0 --angle 0 --velocity 0 --angle-kp 25 --velocity-kp 0.2 --i-kp 3 --i-ki 1300\n"
        << "  testbench_command --iface vcan1.0 dyn200 start|stop|acq-rate N|pub-rate N|zero|read-status|modbus-address N|modbus-baud N\n"
        << "  testbench_command --iface vcan1.0 brake set VALUE|raw VALUE|volts VALUE|off|estop|disable\n\n"
        << "Options:\n"
        << "  --iface NAME          SocketCAN interface to open. Default: vcan1.0\n"
        << "  --local-node-id N     Local Cyphal node ID for published commands. Default: 101\n"
        << "  --node-id N           Target motor node ID for motor commands. Default: 11\n"
        << "  --wait-ms N           Post-command observation/response wait. Default: 1000\n"
        << "  -h, --help            Show this help.\n\n"
        << "Responses:\n"
        << "  motor enable/disable waits for uavcan.register.Access service 384 response.\n"
        << "  motor foc publishes voltbro.foc.command.1.0 on subject 2107 + node_id.\n"
        << "  brake publishes uavcan.primitive.scalar.Real32.1.0 on subject 5103; value is normalized 0.0..1.0.\n"
        << "  brake volts maps 0.0..10.0 V to normalized output; off/estop/disable publish value=0.0.\n"
        << "  brake transfer-id is persisted under /tmp so repeated one-shot CLI calls are not Cyphal duplicates.\n"
        << "  other commands publish a Cyphal command and then print decoded bus summary if traffic is seen.\n\n";
    printSummaryFormat();
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
        const int wait_ms = optionInt(argc, argv, "--wait-ms", 1000);
        auto args = positional(argc, argv);
        if (args.size() < 2) {
            usage();
            return 2;
        }

        const auto local_node_id = static_cast<uint8_t>(optionInt(argc, argv, "--local-node-id", 101));
        if (local_node_id > 127) {
            std::cerr << "--local-node-id must be in 0..127\n";
            return 2;
        }
        auto bus = makeCyphalInterface(iface, local_node_id);
        VbdriveClient motor(bus);
        Dyn200Client dyn(bus);
        BrakeClient brake(bus, loadBrakeTransferId(iface, local_node_id));
        BusSnapshot snapshot;
        attachSnapshotCallbacks(snapshot, motor, dyn);

        const auto& target = args[0];
        const auto& cmd = args[1];
        if (target == "motor") {
            if (node_id == 0 || node_id > 127) {
                std::cerr << "--node-id must be in 1..127\n";
                return 2;
            }
            if (cmd == "enable" || cmd == "disable") {
                const bool enabled = cmd == "enable";
                auto result = motor.setMotorEnabledAndWait(node_id, enabled, std::chrono::milliseconds(wait_ms));
                if (!result) {
                    std::cerr << "No uavcan.register.Access response from node " << unsigned(node_id) << "\n";
                    return 1;
                }
                std::cout << "state.is_on response: mutable=" << result->mutable_register
                          << " persistent=" << result->persistent_register;
                if (result->bit_value) {
                    std::cout << " value=" << (*result->bit_value ? "true" : "false");
                }
                std::cout << "\n";
            } else if (cmd == "velocity" && args.size() > 2) motor.setVelocity(node_id, std::stof(args[2]));
            else if (cmd == "torque" && args.size() > 2) motor.setTorque(node_id, std::stof(args[2]));
            else if (cmd == "position" && args.size() > 2) motor.setPosition(node_id, std::stof(args[2]));
            else if (cmd == "voltage" && args.size() > 2) motor.setVoltage(node_id, std::stof(args[2]));
            else if (cmd == "foc") {
                VbdriveFocCommand foc;
                foc.torque_Nm = static_cast<float>(optionDouble(argc, argv, "--torque", 0.0));
                foc.angle_rad = static_cast<float>(optionDouble(argc, argv, "--angle", 0.0));
                foc.velocity_rad_s = static_cast<float>(optionDouble(argc, argv, "--velocity", 0.0));
                foc.angle_kp = static_cast<float>(optionDouble(argc, argv, "--angle-kp", 25.0));
                foc.velocity_kp = static_cast<float>(optionDouble(argc, argv, "--velocity-kp", 0.2));
                foc.current_kp = static_cast<float>(optionDouble(argc, argv, "--i-kp", 3.0));
                foc.current_ki = static_cast<float>(optionDouble(argc, argv, "--i-ki", 1300.0));
                motor.sendFocCommand(node_id, foc);
                std::cout << "FOC command sent: torque=" << foc.torque_Nm
                          << " angle=" << foc.angle_rad
                          << " velocity=" << foc.velocity_rad_s
                          << " angle_kp=" << foc.angle_kp
                          << " velocity_kp=" << foc.velocity_kp
                          << " I_kp=" << foc.current_kp
                          << " I_ki=" << foc.current_ki << "\n";
            }
            else if (cmd == "stop") motor.stopMotor(node_id);
            else {
                usage();
                return 2;
            }
        } else if (target == "dyn200") {
            if (cmd == "start") dyn.startAcquisition();
            else if (cmd == "stop") dyn.stopAcquisition();
            else if (cmd == "acq-rate" && args.size() > 2) dyn.setAcquisitionRate(static_cast<uint32_t>(std::stoul(args[2])));
            else if (cmd == "pub-rate" && args.size() > 2) dyn.setPublicationRate(static_cast<uint32_t>(std::stoul(args[2])));
            else if (cmd == "zero") {
                std::cout << "WARNING: sending explicit DYN-200 sensor zero request.\n";
                dyn.requestZero();
            } else if (cmd == "read-status") dyn.readStatus();
            else if (cmd == "modbus-address" && args.size() > 2) dyn.setRuntimeModbusAddress(static_cast<uint8_t>(std::stoul(args[2])));
            else if (cmd == "modbus-baud" && args.size() > 2) dyn.setRuntimeModbusBaudrate(static_cast<uint32_t>(std::stoul(args[2])));
            else {
                usage();
                return 2;
            }
        } else if (target == "brake") {
            if ((cmd == "set" || cmd == "raw") && args.size() > 2) brake.setRaw(std::stof(args[2]));
            else if (cmd == "volts" && args.size() > 2) brake.setVoltage(std::stof(args[2]));
            else if (cmd == "off" || cmd == "estop" || cmd == "disable") brake.disable();
            else {
                usage();
                return 2;
            }
            saveBrakeTransferId(iface, local_node_id, brake.transferId());
            std::cout << "Brake command sent from node " << unsigned(local_node_id)
                      << " on subject 5103 as uavcan.primitive.scalar.Real32.1.0";
            if ((cmd == "set" || cmd == "raw") && args.size() > 2) {
                std::cout << " value=" << std::stof(args[2]);
            } else if (cmd == "volts" && args.size() > 2) {
                std::cout << " value=" << (std::stof(args[2]) / 10.0F);
            } else {
                std::cout << " value=0";
            }
            std::cout << ". Firmware has no direct Cyphal response for brake command.\n";
        } else {
            usage();
            return 2;
        }

        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::milliseconds(wait_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            bus->loop();
        }
        printSummary(snapshot, std::chrono::steady_clock::now() - start);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
