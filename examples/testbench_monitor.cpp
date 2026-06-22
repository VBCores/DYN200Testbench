#include "testbench_cli_common.hpp"

#include <fstream>
#include <iostream>

using namespace voltbro::testbench;
using namespace testbench_cli;

namespace {

void writeMotorHeader(std::ofstream& out) {
    out << "host_time_ns,iface,source_node_id,subject_id,transfer_id,timestamp_us,"
           "position_rad,velocity_rad_s,torque_Nm,current_A,voltage_V,"
           "temperature_C,mcu_temperature_C,stator_temperature_C,has_fault\n";
}

void writeDynHeader(std::ofstream& out) {
    out << "host_time_ns,iface,source,source_node_id,subject_id,transfer_id,"
           "sample_counter,timestamp_us,angular_velocity_rad_s,torque_Nm,power_W,"
           "raw_speed,raw_torque,raw_power,status_flags,actual_acq_rate_hz,"
           "publication_rate_hz,crc_error_count,timeout_count,uart_error_count,notes\n";
}

template <typename T>
std::string valueOrEmpty(const std::optional<T>& v) {
    return v ? std::to_string(*v) : std::string{};
}

void usage() {
    std::cout
        << "usage: testbench_monitor [--iface vcan1.0] [--mode summary|csv] [options]\n\n"
        << "Monitor one shared testbench Cyphal/CAN bus.\n\n"
        << "Options:\n"
        << "  --iface NAME          SocketCAN interface to open. Default: vcan1.0\n"
        << "  --mode summary|csv    summary prints live counters; csv also writes CSV files. Default: summary\n"
        << "  --duration N          Run time in seconds; 0 means until Ctrl+C. Default: 0\n"
        << "  --period-ms N         Summary print period. Default: 1000\n"
        << "  --motor-csv PATH      Motor CSV path in csv mode. Default: motor.csv\n"
        << "  --dyn-csv PATH        DYN-200/brake CSV path in csv mode. Default: dyn200_brake.csv\n"
        << "  -h, --help            Show this help.\n\n"
        << "CSV files:\n"
        << "  motor CSV contains VBDRIVE state_simple subject 3811.\n"
        << "  dyn CSV contains DYN-200 state/status subjects 5100 and 5101.\n\n";
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
        const auto mode = optionValue(argc, argv, "--mode", "summary");
        const int duration_s = optionInt(argc, argv, "--duration", 0);
        const int period_ms = optionInt(argc, argv, "--period-ms", 1000);
        const auto motor_csv = optionValue(argc, argv, "--motor-csv", "motor.csv");
        const auto dyn_csv = optionValue(argc, argv, "--dyn-csv", "dyn200_brake.csv");

        auto bus = makeCyphalInterface(iface, 100);
        BusSnapshot snapshot;
        HeartbeatSubscription heartbeat(bus, snapshot);
        VbdriveClient motor(bus);
        Dyn200Client dyn(bus);

        std::ofstream motor_out;
        std::ofstream dyn_out;
        if (mode == "csv") {
            motor_out.open(motor_csv);
            dyn_out.open(dyn_csv);
            writeMotorHeader(motor_out);
            writeDynHeader(dyn_out);
            std::cout << "Writing " << motor_csv << " and " << dyn_csv << "\n";
        } else if (mode != "summary") {
            usage();
            return 2;
        }

        motor.onState([&](const VbdriveState& s) {
            snapshot.vbdrive_state++;
            snapshot.vbdrive_nodes.insert(s.source_node_id);
            snapshot.last_motor = s;
            if (motor_out.is_open()) {
                motor_out << steadyTimeNs(s.host_receive_time) << "," << iface << ","
                          << unsigned(s.source_node_id) << "," << kVbdriveStateSimpleSubjectId << ","
                          << s.transfer_id << "," << valueOrEmpty(s.timestamp_us) << ","
                          << valueOrEmpty(s.position_rad) << "," << valueOrEmpty(s.velocity_rad_s) << ","
                          << valueOrEmpty(s.torque_Nm) << "," << valueOrEmpty(s.current_A) << ","
                          << valueOrEmpty(s.voltage_V) << "," << valueOrEmpty(s.temperature_C) << ","
                          << valueOrEmpty(s.mcu_temperature_C) << "," << valueOrEmpty(s.stator_temperature_C)
                          << "," << (s.has_fault ? 1 : 0) << "\n";
            }
        });
        dyn.onState([&](const Dyn200State& s) {
            snapshot.dyn_state++;
            snapshot.dyn200_nodes.insert(s.source_node_id);
            snapshot.last_dyn_state = s;
            if (dyn_out.is_open()) {
                dyn_out << steadyTimeNs(s.host_receive_time) << "," << iface << ",dyn200_state,"
                        << unsigned(s.source_node_id) << "," << kDyn200StateSubjectId << ","
                        << unsigned(s.transfer_id) << "," << s.sample_counter << "," << s.timestamp_us
                        << "," << s.angular_velocity_rad_s << "," << s.torque_Nm << "," << s.power_W
                        << "," << s.raw_speed << "," << s.raw_torque << "," << s.raw_power << ","
                        << unsigned(s.status_flags) << ",,,,,ok\n";
            }
        });
        dyn.onStatus([&](const Dyn200Status& s) {
            snapshot.dyn_status++;
            snapshot.dyn200_nodes.insert(s.source_node_id);
            snapshot.last_dyn_status = s;
            if (dyn_out.is_open()) {
                dyn_out << steadyTimeNs(s.host_receive_time) << "," << iface << ",dyn200_status,"
                        << unsigned(s.source_node_id) << "," << kDyn200StatusSubjectId << ","
                        << unsigned(s.transfer_id) << "," << s.sample_counter << ",,,,,,,,,"
                        << s.actual_acquisition_rate_hz << "," << s.cyphal_publication_rate_hz << ","
                        << s.crc_error_count << "," << s.timeout_count << "," << s.uart_error_count
                        << ",ok\n";
            }
        });

        const auto start = std::chrono::steady_clock::now();
        auto next_print = start;
        while (!g_stop) {
            bus->loop();
            const auto now = std::chrono::steady_clock::now();
            if (duration_s > 0 && now - start >= std::chrono::seconds(duration_s)) {
                break;
            }
            if (now >= next_print) {
                printSummary(snapshot, now - start);
                next_print = now + std::chrono::milliseconds(period_ms);
            }
        }
        if (motor_out.is_open()) motor_out.flush();
        if (dyn_out.is_open()) dyn_out.flush();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
