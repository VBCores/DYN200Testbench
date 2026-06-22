#pragma once

#include "brake.hpp"
#include "config.hpp"
#include "csv_logger.hpp"
#include "cyphal_node.hpp"
#include "dyn200.hpp"
#include "errors.hpp"
#include "statistics.hpp"
#include "time.hpp"
#include "vbdrive.hpp"

#include <uavcan/node/Heartbeat_1_0.hpp>

#include <chrono>
#include <memory>

namespace voltbro::testbench {

class TestbenchClient {
public:
    using Config = ClientConfig;

    explicit TestbenchClient(Config cfg)
        : cfg_(std::move(cfg)),
          motor_iface_(makeCyphalInterface(cfg_.motor_iface, cfg_.local_node_id)),
          dyn_iface_(cfg_.dyn_iface == cfg_.motor_iface
                         ? motor_iface_
                         : makeCyphalInterface(cfg_.dyn_iface, cfg_.local_node_id)),
          motor_heartbeat_sub_(std::make_unique<HeartbeatSubscription>(motor_cyphal_, motor_iface_)),
          dyn_heartbeat_sub_(cfg_.dyn_iface == cfg_.motor_iface
                                 ? nullptr
                                 : std::make_unique<HeartbeatSubscription>(dyn_cyphal_, dyn_iface_)),
          motor_(motor_iface_),
          dyn200_(dyn_iface_),
          brake_(dyn_iface_) {}

    void connect() {}
    void close() {
        motor_iface_.reset();
        dyn_iface_.reset();
    }

    bool checkMotorAlive(std::chrono::milliseconds timeout) {
        const auto before_heartbeat = motor_cyphal_.heartbeat_rx;
        const auto before_state = motor_.statistics().state_messages;
        spinFor(timeout);
        return motor_cyphal_.heartbeat_rx > before_heartbeat ||
               motor_.statistics().state_messages > before_state;
    }

    bool checkDyn200Alive(std::chrono::milliseconds timeout) {
        const auto before_heartbeat = dyn_cyphal_.heartbeat_rx;
        const auto before_state = dyn200_.statistics().state_messages;
        const auto before_status = dyn200_.statistics().status_messages;
        spinFor(timeout);
        const auto after = dyn200_.statistics();
        return dyn_cyphal_.heartbeat_rx > before_heartbeat ||
               after.state_messages > before_state ||
               after.status_messages > before_status;
    }

    void spinOnce(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        do {
            motor_iface_->loop();
            if (dyn_iface_ != motor_iface_) {
                dyn_iface_->loop();
            }
        } while (timeout.count() > 0 && std::chrono::steady_clock::now() < deadline);
    }

    void spinFor(std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            spinOnce();
        }
    }

    VbdriveClient& motor() { return motor_; }
    Dyn200Client& dyn200() { return dyn200_; }
    BrakeClient& brake() { return brake_; }
    CyphalInterfacePtr motorInterface() { return motor_iface_; }
    CyphalInterfacePtr dynInterface() { return dyn_iface_; }

    ClientStatistics statistics() const {
        ClientStatistics s;
        s.motor_cyphal = motor_cyphal_;
        s.dyn_cyphal = dyn_iface_ == motor_iface_ ? motor_cyphal_ : dyn_cyphal_;
        s.dyn200 = dyn200_.statistics();
        s.vbdrive = motor_.statistics();
        return s;
    }

private:
    class HeartbeatSubscription : public AbstractSubscription<uavcan_node_Heartbeat_1_0> {
    public:
        HeartbeatSubscription(CyphalStatistics& stats, InterfacePtr& interface)
            : AbstractSubscription<uavcan_node_Heartbeat_1_0>(interface, kHeartbeatSubjectId),
              stats_(stats) {}

    private:
        void handler(const uavcan_node_Heartbeat_1_0&, CanardRxTransfer*) override {
            stats_.heartbeat_rx++;
            stats_.transfers_rx++;
        }

        CyphalStatistics& stats_;
    };

    Config cfg_;
    CyphalInterfacePtr motor_iface_;
    CyphalInterfacePtr dyn_iface_;
    CyphalStatistics motor_cyphal_{};
    CyphalStatistics dyn_cyphal_{};
    std::unique_ptr<HeartbeatSubscription> motor_heartbeat_sub_;
    std::unique_ptr<HeartbeatSubscription> dyn_heartbeat_sub_;
    VbdriveClient motor_;
    Dyn200Client dyn200_;
    BrakeClient brake_;
};

}  // namespace voltbro::testbench
