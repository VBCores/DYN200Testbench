#include "common_cli.hpp"

#include <voltbro_testbench_client/all.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

using namespace voltbro::testbench;

namespace {

struct Stats {
    uint64_t count{};
    double sum{};
    double sum_sq{};
    double min{std::numeric_limits<double>::infinity()};
    double max{-std::numeric_limits<double>::infinity()};

    void add(double value) {
        if (!std::isfinite(value)) {
            return;
        }
        ++count;
        sum += value;
        sum_sq += value * value;
        if (value < min) min = value;
        if (value > max) max = value;
    }

    double mean() const {
        return count > 0 ? sum / static_cast<double>(count) : std::numeric_limits<double>::quiet_NaN();
    }

    double stddev() const {
        if (count < 2) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        const double m = mean();
        const double variance = (sum_sq / static_cast<double>(count)) - (m * m);
        return std::sqrt(variance > 0.0 ? variance : 0.0);
    }
};

struct SweepRow {
    double command_torque{};
    std::optional<double> state_errors_before;
    std::optional<double> state_errors_after;
    Stats motor_torque;
    Stats dyn200_torque;
};

VbdriveFocCommand makeTorqueCommand(float torque_Nm, float current_kp, float current_ki) {
    VbdriveFocCommand cmd;
    cmd.torque_Nm = torque_Nm;
    cmd.angle_rad = 0.0F;
    cmd.velocity_rad_s = 0.0F;
    cmd.angle_kp = 0.0F;
    cmd.velocity_kp = 0.0F;
    cmd.current_kp = current_kp;
    cmd.current_ki = current_ki;
    return cmd;
}

void usage() {
    std::cout
        << "usage: testbench_torque_sweep --arm [options]\n\n"
        << "Sweep motor FOC torque and write averaged motor/DYN-200 torque response to CSV.\n\n"
        << "Options:\n"
        << "  --arm                 Required. Enables the motor and runs the sweep.\n"
        << "  --iface NAME          SocketCAN interface. Default: vcan1.0\n"
        << "  --local-node-id N     Local Cyphal node ID. Default: 101\n"
        << "  --node-id N           Target motor node ID. Default: 11\n"
        << "  --min VALUE           First torque command, Nm. Default: 0\n"
        << "  --max VALUE           Last torque command, Nm. Default: 100\n"
        << "  --step VALUE          Torque command step, Nm. Default: 5\n"
        << "  --settle-ms N         Settle time before sampling each point. Default: 300\n"
        << "  --sample-ms N         Sampling time for each point. Default: 700\n"
        << "  --command-period-ms N Re-send FOC command period. Default: 100\n"
        << "  --i-kp VALUE          FOC current Kp. Default: 5\n"
        << "  --i-ki VALUE          FOC current Ki. Default: 1300\n"
        << "  --out PATH            Output CSV. Default: torque_sweep.csv\n"
        << "  --leave-enabled       Do not disable the motor at the end.\n"
        << "  -h, --help            Show this help.\n";
}

void writeHeader(std::ofstream& out) {
    out << "command_torque_Nm,"
           "motor_torque_mean_Nm,dyn200_torque_mean_Nm,"
           "motor_torque_stddev_Nm,dyn200_torque_stddev_Nm,"
           "motor_torque_min_Nm,motor_torque_max_Nm,"
           "dyn200_torque_min_Nm,dyn200_torque_max_Nm,"
           "motor_samples,dyn200_samples,"
           "state_errors_before,state_errors_after,state_errors_delta\n";
}

double optionalOrNan(const std::optional<double>& value) {
    return value ? *value : std::numeric_limits<double>::quiet_NaN();
}

void writeRow(std::ofstream& out, const SweepRow& row) {
    std::optional<double> errors_delta;
    if (row.state_errors_before && row.state_errors_after) {
        errors_delta = *row.state_errors_after - *row.state_errors_before;
    }
    out << row.command_torque << ","
        << row.motor_torque.mean() << ","
        << row.dyn200_torque.mean() << ","
        << row.motor_torque.stddev() << ","
        << row.dyn200_torque.stddev() << ","
        << (row.motor_torque.count ? row.motor_torque.min : std::numeric_limits<double>::quiet_NaN()) << ","
        << (row.motor_torque.count ? row.motor_torque.max : std::numeric_limits<double>::quiet_NaN()) << ","
        << (row.dyn200_torque.count ? row.dyn200_torque.min : std::numeric_limits<double>::quiet_NaN()) << ","
        << (row.dyn200_torque.count ? row.dyn200_torque.max : std::numeric_limits<double>::quiet_NaN()) << ","
        << row.motor_torque.count << ","
        << row.dyn200_torque.count << ","
        << optionalOrNan(row.state_errors_before) << ","
        << optionalOrNan(row.state_errors_after) << ","
        << optionalOrNan(errors_delta) << "\n";
}

bool syntaxValid(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--arm" || arg == "--leave-enabled") {
            continue;
        }
        if (arg == "--iface" || arg == "--local-node-id" || arg == "--node-id" ||
            arg == "--min" || arg == "--max" || arg == "--step" ||
            arg == "--settle-ms" || arg == "--sample-ms" || arg == "--command-period-ms" ||
            arg == "--i-kp" || arg == "--i-ki" || arg == "--out") {
            if (i + 1 >= argc) {
                return false;
            }
            ++i;
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (wantsHelp(argc, argv)) {
            usage();
            return 0;
        }
        if (!syntaxValid(argc, argv)) {
            return invalidSyntax("testbench_torque_sweep");
        }
        if (!hasFlag(argc, argv, "--arm")) {
            std::cerr << "Refusing to run: pass --arm to enable the motor and execute the sweep.\n";
            return 2;
        }

        installSignalHandlers();
        const auto iface = optionValue(argc, argv, "--iface", "vcan1.0");
        const auto local_node_id = static_cast<uint8_t>(optionInt(argc, argv, "--local-node-id", 101));
        const auto node_id = static_cast<uint8_t>(optionInt(argc, argv, "--node-id", 11));
        const double min_torque = optionDouble(argc, argv, "--min", 0.0);
        const double max_torque = optionDouble(argc, argv, "--max", 100.0);
        const double step_torque = optionDouble(argc, argv, "--step", 5.0);
        const int settle_ms = optionInt(argc, argv, "--settle-ms", 300);
        const int sample_ms = optionInt(argc, argv, "--sample-ms", 700);
        const int command_period_ms = optionInt(argc, argv, "--command-period-ms", 100);
        const auto current_kp = static_cast<float>(optionDouble(argc, argv, "--i-kp", 5.0));
        const auto current_ki = static_cast<float>(optionDouble(argc, argv, "--i-ki", 1300.0));
        const auto out_path = optionValue(argc, argv, "--out", "torque_sweep.csv");
        const bool leave_enabled = hasFlag(argc, argv, "--leave-enabled");

        if (local_node_id > 127 || node_id == 0 || node_id > 127 ||
            step_torque <= 0.0 || min_torque > max_torque ||
            settle_ms < 0 || sample_ms <= 0 || command_period_ms <= 0) {
            return invalidSyntax("testbench_torque_sweep");
        }

        std::ofstream out(out_path);
        if (!out) {
            std::cerr << "ERROR: cannot open " << out_path << " for writing\n";
            return 1;
        }
        writeHeader(out);

        auto bus = makeCyphalInterface(iface, local_node_id);
        VbdriveClient motor(bus);
        Dyn200Client dyn200(bus);

        SweepRow* active_row = nullptr;
        motor.onState([&](const VbdriveState& state) {
            if (active_row && state.torque_Nm) {
                active_row->motor_torque.add(*state.torque_Nm);
            }
        });
        dyn200.onState([&](const Dyn200State& state) {
            if (active_row) {
                active_row->dyn200_torque.add(state.torque_Nm);
            }
        });

        auto enable_result = motor.setMotorEnabledAndWait(node_id, true, std::chrono::milliseconds(1000));
        if (!enable_result || !enable_result->bit_value || !*enable_result->bit_value) {
            std::cerr << "ERROR: state.is_on was not confirmed by node " << unsigned(node_id) << "\n";
            return 1;
        }

        dyn200.startAcquisition();
        dyn200.setPublicationRate(50);

        auto readStateErrors = [&]() -> std::optional<double> {
            auto result = motor.readRegisterAndWait(node_id, "state.errors", std::chrono::milliseconds(300));
            if (!result) {
                return std::nullopt;
            }
            if (result->natural_value) {
                return static_cast<double>(*result->natural_value);
            }
            if (result->integer_value) {
                return static_cast<double>(*result->integer_value);
            }
            return std::nullopt;
        };

        auto sendRepeated = [&](const VbdriveFocCommand& command,
                                std::chrono::milliseconds duration,
                                SweepRow* row) {
            active_row = row;
            const auto deadline = std::chrono::steady_clock::now() + duration;
            auto next_tx = std::chrono::steady_clock::now();
            while (!g_stop && std::chrono::steady_clock::now() < deadline) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= next_tx) {
                    motor.sendFocCommand(node_id, command);
                    next_tx = now + std::chrono::milliseconds(command_period_ms);
                }
                bus->loop();
            }
            active_row = nullptr;
        };

        for (double torque = min_torque; !g_stop && torque <= max_torque + (step_torque * 0.001); torque += step_torque) {
            const auto command = makeTorqueCommand(static_cast<float>(torque), current_kp, current_ki);
            SweepRow row;
            row.command_torque = torque;
            row.state_errors_before = readStateErrors();

            sendRepeated(command, std::chrono::milliseconds(settle_ms), nullptr);

            sendRepeated(command, std::chrono::milliseconds(sample_ms), &row);
            row.state_errors_after = readStateErrors();
            writeRow(out, row);
            out.flush();
        }

        motor.stopMotor(node_id);
        if (!leave_enabled) {
            motor.disableMotor(node_id);
        }
        dyn200.stopAcquisition();
        flushCyphalTx(bus);

        std::cout << "OK: wrote " << out_path << "\n";
        return g_stop ? 130 : 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
