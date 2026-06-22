#pragma once

#include "common_cli.hpp"
#include "voltbro_testbench_client/all.hpp"
#include "voltbro_testbench_client/time.hpp"

#include <uavcan/node/Heartbeat_1_0.hpp>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace testbench_cli {

using namespace voltbro::testbench;

struct BusSnapshot {
    std::map<uint8_t, uint64_t> heartbeat_by_node;
    std::set<uint8_t> vbdrive_nodes;
    std::set<uint8_t> dyn200_nodes;
    uint64_t vbdrive_state{};
    uint64_t dyn_state{};
    uint64_t dyn_status{};
    std::optional<VbdriveState> last_motor;
    std::optional<Dyn200State> last_dyn_state;
    std::optional<Dyn200Status> last_dyn_status;
};

class HeartbeatSubscription : public AbstractSubscription<uavcan_node_Heartbeat_1_0> {
public:
    HeartbeatSubscription(CyphalInterfacePtr& interface, BusSnapshot& snapshot)
        : AbstractSubscription<uavcan_node_Heartbeat_1_0>(interface, kHeartbeatSubjectId),
          snapshot_(snapshot) {}

private:
    void handler(const uavcan_node_Heartbeat_1_0&, CanardRxTransfer* transfer) override {
        if (transfer != nullptr && transfer->metadata.remote_node_id <= CANARD_NODE_ID_MAX) {
            snapshot_.heartbeat_by_node[static_cast<uint8_t>(transfer->metadata.remote_node_id)]++;
        }
    }

    BusSnapshot& snapshot_;
};

inline void attachSnapshotCallbacks(BusSnapshot& snapshot, VbdriveClient& motor, Dyn200Client& dyn) {
    motor.onState([&](const VbdriveState& s) {
        snapshot.vbdrive_state++;
        snapshot.vbdrive_nodes.insert(s.source_node_id);
        snapshot.last_motor = s;
    });
    dyn.onState([&](const Dyn200State& s) {
        snapshot.dyn_state++;
        snapshot.dyn200_nodes.insert(s.source_node_id);
        snapshot.last_dyn_state = s;
    });
    dyn.onStatus([&](const Dyn200Status& s) {
        snapshot.dyn_status++;
        snapshot.dyn200_nodes.insert(s.source_node_id);
        snapshot.last_dyn_status = s;
    });
}

inline double rate(uint64_t count, std::chrono::steady_clock::duration elapsed) {
    const auto seconds = std::chrono::duration<double>(elapsed).count();
    return seconds > 0.0 ? static_cast<double>(count) / seconds : 0.0;
}

inline std::string nodeList(const std::set<uint8_t>& nodes) {
    std::string out;
    for (const auto node : nodes) {
        if (!out.empty()) out += ",";
        out += std::to_string(unsigned(node));
    }
    return out.empty() ? "-" : out;
}

inline void printKnownPorts() {
    std::cout << "Known Cyphal ports:\n"
              << "  7509 heartbeat uavcan.node.Heartbeat.1.0\n"
              << "  3811 pub  voltbro.foc.state_simple.1.0\n"
              << "  " << kVbdriveCommandBaseSubjectId << "+node pub voltbro.foc.command.1.0\n"
              << "  " << kVbdriveSpecificControlBaseSubjectId << "+node pub voltbro.foc.specific_control.1.0\n"
              << "  384  srv  uavcan.register.Access.1.0 for motor state.is_on\n"
              << "  5100 pub  voltbro.dynamometer.state.1.0\n"
              << "  5101 pub  voltbro.dynamometer.status.1.0\n"
              << "  5102 pub  voltbro.dynamometer.command.1.0\n"
              << "  5103 pub  uavcan.primitive.scalar.Real32.1.0\n";
}

inline void printSummaryFormat() {
    std::cout
        << "Summary format:\n"
        << "  devices: application-level node IDs inferred from decoded testbench topics.\n"
        << "  nodes_by_heartbeat: OpenCyphal heartbeat publishers on subject 7509; format node(rx_count).\n"
        << "  application_publishers: decoded testbench publications; format name[subject]=count(rate_hz).\n"
        << "  motor/dyn200: latest decoded application sample values.\n";
}

inline void printSummary(const BusSnapshot& snapshot, std::chrono::steady_clock::duration elapsed) {
    std::cout << std::fixed << std::setprecision(2)
              << "devices: vbdrive_nodes=" << nodeList(snapshot.vbdrive_nodes)
              << " dyn200_nodes=" << nodeList(snapshot.dyn200_nodes) << "\n"
              << "nodes_by_heartbeat: ";
    if (snapshot.heartbeat_by_node.empty()) {
        std::cout << "-";
    } else {
        bool first = true;
        for (const auto& [node, count] : snapshot.heartbeat_by_node) {
            if (!first) std::cout << ",";
            std::cout << unsigned(node) << "(rx=" << count << ")";
            first = false;
        }
    }
    std::cout << "\napplication_publishers: vbdrive_state[" << kVbdriveStateSimpleSubjectId
              << "]=" << snapshot.vbdrive_state
              << "(" << rate(snapshot.vbdrive_state, elapsed) << " Hz)"
              << " dyn_state[" << kDyn200StateSubjectId << "]=" << snapshot.dyn_state
              << "(" << rate(snapshot.dyn_state, elapsed) << " Hz)"
              << " dyn_status[" << kDyn200StatusSubjectId << "]=" << snapshot.dyn_status
              << "(" << rate(snapshot.dyn_status, elapsed) << " Hz)\n";
    if (snapshot.last_motor) {
        const auto& s = *snapshot.last_motor;
        std::cout << "motor: src=" << unsigned(s.source_node_id)
                  << " pos=" << s.position_rad.value_or(0.0F)
                  << " vel=" << s.velocity_rad_s.value_or(0.0F)
                  << " torque=" << s.torque_Nm.value_or(0.0F)
                  << " fault=" << s.has_fault << "\n";
    }
    if (snapshot.last_dyn_state) {
        const auto& s = *snapshot.last_dyn_state;
        std::cout << "dyn200: src=" << unsigned(s.source_node_id)
                  << " sample=" << s.sample_counter
                  << " speed=" << s.angular_velocity_rad_s
                  << " torque=" << s.torque_Nm
                  << " flags=" << unsigned(s.status_flags) << "\n";
    }
}

}  // namespace testbench_cli
