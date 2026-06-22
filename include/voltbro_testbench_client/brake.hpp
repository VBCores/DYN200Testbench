#pragma once

#include "cyphal_node.hpp"
#include "errors.hpp"

#include <uavcan/primitive/scalar/Real32_1_0.hpp>

#include <cmath>
#include <string>
#include <utility>

namespace voltbro::testbench {

class BrakeClient {
public:
    explicit BrakeClient(CyphalInterfacePtr interface, CanardTransferID initial_transfer_id = 0)
        : interface_(std::move(interface)), transfer_id_(initial_transfer_id) {}

    void setNormalized(float raw_0_to_1) {
        validateFiniteRange(raw_0_to_1, 0.0F, 1.0F, "brake raw command");
        send(raw_0_to_1);
    }
    void setVoltage(float brake_side_volts) {
        validateFiniteRange(brake_side_volts, 0.0F, 10.0F, "brake voltage command");
        setNormalized(brake_side_volts / 10.0F);
    }
    void setRaw(float raw_0_to_1) { setNormalized(raw_0_to_1); }
    void disable() { send(0.0F); }
    void off() { disable(); }
    void emergencyStop() { disable(); }
    CanardTransferID transferId() const { return transfer_id_; }

private:
    static void validateFiniteRange(float value, float min_value, float max_value, const char* name) {
        if (!std::isfinite(value) || value < min_value || value > max_value) {
            throw ProtocolError(std::string(name) + " must be finite and in range " +
                                std::to_string(min_value) + ".." + std::to_string(max_value));
        }
    }

    void send(float raw_0_to_1) {
        uavcan_primitive_scalar_Real32_1_0 msg{};
        msg.value = raw_0_to_1;
        interface_->send_msg(&msg, kBrakeCommandSubjectId, &transfer_id_);
        flushCyphalTx(interface_);
    }

    CyphalInterfacePtr interface_;
    CanardTransferID transfer_id_{};
};

}  // namespace voltbro::testbench
