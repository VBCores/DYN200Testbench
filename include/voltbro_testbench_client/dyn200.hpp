#pragma once

#include "cyphal_node.hpp"

#include <cyphal/subscriptions/subscription.h>
#include <uavcan/primitive/scalar/Real32_1_0.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace voltbro::testbench {

// Speed and torque are independent publications. A pair is not an atomic sample.
struct Dyn200Measurement {
    float value{};
    uint8_t source_node_id{};
    uint8_t transfer_id{};
    std::chrono::steady_clock::time_point host_receive_time{};
};

class Dyn200Client {
public:
    explicit Dyn200Client(CyphalInterfacePtr interface, uint8_t node_id = kDyn200NodeId)
        : interface_(std::move(interface)), node_id_(node_id),
          speed_sub_(*this, interface_, kDyn200SpeedSubjectId, true),
          torque_sub_(*this, interface_, kDyn200TorqueSubjectId, false) {
        if (node_id_ == 0 || node_id_ > CANARD_NODE_ID_MAX) {
            throw ProtocolError("DYN-200 node ID must be in range 1..127");
        }
    }

    // Speed is in rpm. Torque is in N*m, scaled by the firmware.
    void onSpeed(std::function<void(const Dyn200Measurement&)> callback) { speed_cb_ = std::move(callback); }
    void onTorque(std::function<void(const Dyn200Measurement&)> callback) { torque_cb_ = std::move(callback); }
    std::optional<Dyn200Measurement> lastSpeed() const { return speed_; }
    std::optional<Dyn200Measurement> lastTorque() const { return torque_; }

private:
    class MeasurementSubscription final : public AbstractSubscription<uavcan_primitive_scalar_Real32_1_0> {
    public:
        MeasurementSubscription(Dyn200Client& owner, InterfacePtr& interface, CanardPortID port, bool speed)
            : AbstractSubscription<uavcan_primitive_scalar_Real32_1_0>(interface, port),
              owner_(owner), speed_(speed) {}

    private:
        void handler(const uavcan_primitive_scalar_Real32_1_0& message, CanardRxTransfer* transfer) override {
            if (transfer == nullptr || transfer->metadata.remote_node_id != owner_.node_id_) return;
            Dyn200Measurement measurement{
                message.value,
                static_cast<uint8_t>(transfer->metadata.remote_node_id),
                transfer->metadata.transfer_id,
                std::chrono::steady_clock::now()
            };
            if (speed_) {
                owner_.speed_ = measurement;
                if (owner_.speed_cb_) owner_.speed_cb_(measurement);
            } else {
                owner_.torque_ = measurement;
                if (owner_.torque_cb_) owner_.torque_cb_(measurement);
            }
        }

        Dyn200Client& owner_;
        bool speed_;
    };

    CyphalInterfacePtr interface_;
    uint8_t node_id_;
    MeasurementSubscription speed_sub_;
    MeasurementSubscription torque_sub_;
    std::optional<Dyn200Measurement> speed_;
    std::optional<Dyn200Measurement> torque_;
    std::function<void(const Dyn200Measurement&)> speed_cb_;
    std::function<void(const Dyn200Measurement&)> torque_cb_;
};

}  // namespace voltbro::testbench
