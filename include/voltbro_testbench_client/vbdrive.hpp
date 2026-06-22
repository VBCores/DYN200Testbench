#pragma once

#include "cyphal_node.hpp"
#include "statistics.hpp"

#include <cyphal/subscriptions/subscription.h>
#include <uavcan/_register/Access_1_0.hpp>
#include <voltbro/foc/command_1_0.hpp>
#include <voltbro/foc/specific_control_1_0.hpp>
#include <voltbro/foc/state_simple_1_0.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace voltbro::testbench {

struct VbdriveState {
    uint8_t source_node_id{};
    uint32_t transfer_id{};
    std::optional<uint64_t> timestamp_us;
    std::optional<float> velocity_rad_s;
    std::optional<float> position_rad;
    std::optional<float> torque_Nm;
    std::optional<float> current_A;
    std::optional<float> voltage_V;
    std::optional<float> temperature_C;
    std::optional<float> mcu_temperature_C;
    std::optional<float> stator_temperature_C;
    bool has_fault{};
    std::chrono::steady_clock::time_point host_receive_time{};
};

inline VbdriveState convertVbdriveStateSimple(const voltbro_foc_state_simple_1_0& msg,
                                              const CanardRxTransfer* transfer) {
    VbdriveState s;
    if (transfer != nullptr) {
        s.source_node_id = static_cast<uint8_t>(transfer->metadata.remote_node_id);
        s.transfer_id = transfer->metadata.transfer_id;
    }
    s.timestamp_us = msg.timestamp.microsecond;
    s.position_rad = msg.angle.radian;
    s.velocity_rad_s = msg.velocity.radian_per_second;
    s.torque_Nm = msg._torque.newton_meter;
    s.current_A = msg.current.ampere;
    s.voltage_V = msg.bus_voltage.volt;
    s.mcu_temperature_C = msg.mcu_temp.kelvin - 273.15F;
    s.stator_temperature_C = msg.stator_temp.kelvin - 273.15F;
    s.temperature_C = s.stator_temperature_C;
    s.has_fault = msg.has_fault.value;
    s.host_receive_time = std::chrono::steady_clock::now();
    return s;
}

enum class VbdriveSetpointType : uint8_t {
    Velocity = voltbro_foc_specific_control_1_0_VELOCITY,
    Torque = voltbro_foc_specific_control_1_0_TORQUE,
    Position = voltbro_foc_specific_control_1_0_POSITION,
    Voltage = voltbro_foc_specific_control_1_0_VOLTAGE,
    Universal = voltbro_foc_specific_control_1_0_UNIVERSAL,
};

struct VbdriveFocCommand {
    float torque_Nm{};
    float angle_rad{};
    float velocity_rad_s{};
    float angle_kp{};
    float velocity_kp{};
    float current_kp{};
    float current_ki{};
};

struct VbdriveRegisterAccessResult {
    uint8_t source_node_id{};
    uint8_t transfer_id{};
    bool mutable_register{};
    bool persistent_register{};
    std::optional<bool> bit_value;
};

class VbdriveClient {
public:
    explicit VbdriveClient(CyphalInterfacePtr interface)
        : interface_(std::move(interface)),
          state_sub_(std::make_unique<StateSubscription>(*this, interface_)),
          register_response_sub_(std::make_unique<RegisterAccessResponseSubscription>(*this, interface_)) {}

    void onState(std::function<void(const VbdriveState&)> cb) { state_cb_ = std::move(cb); }
    std::optional<VbdriveState> lastState() const { return last_state_; }
    std::optional<VbdriveRegisterAccessResult> lastRegisterAccess() const { return last_register_access_; }
    VbdriveStatistics statistics() const { return stats_; }

    bool protocolMappingAvailable() const { return true; }
    std::string protocolMappingStatus() const {
        return "Telemetry mapped on 3811. Commands mapped with libcxxcanard: "
               "voltbro.foc.command.1.0 on 2107+node_id, "
               "voltbro.foc.specific_control.1.0 on 3407+node_id, "
               "state.is_on via uavcan.register.Access service 384.";
    }

    void sendSpecificControl(uint8_t target_node_id, VbdriveSetpointType type, float value) {
        validateNodeId(target_node_id);
        voltbro_foc_specific_control_1_0 msg{};
        msg.set_point_type = static_cast<uint8_t>(type);
        msg.set_point_value = value;
        interface_->send_msg(&msg,
                             static_cast<CanardPortID>(kVbdriveSpecificControlBaseSubjectId + target_node_id),
                             &specific_control_tid_);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    void setVelocity(uint8_t target_node_id, float rad_s) {
        VbdriveFocCommand cmd;
        cmd.velocity_rad_s = rad_s;
        cmd.angle_kp = 0.0F;
        cmd.velocity_kp = 2.0F;
        cmd.current_kp = 3.0F;
        cmd.current_ki = 1300.0F;
        sendFocCommand(target_node_id, cmd);
    }

    void setTorque(uint8_t target_node_id, float Nm) {
        sendSpecificControl(target_node_id, VbdriveSetpointType::Torque, Nm);
    }

    void setPosition(uint8_t target_node_id, float rad) {
        VbdriveFocCommand cmd;
        cmd.angle_rad = rad;
        cmd.angle_kp = 25.0F;
        cmd.velocity_kp = 0.2F;
        cmd.current_kp = 3.0F;
        cmd.current_ki = 1300.0F;
        sendFocCommand(target_node_id, cmd);
    }

    void setVoltage(uint8_t target_node_id, float V) {
        sendSpecificControl(target_node_id, VbdriveSetpointType::Voltage, V);
    }

    void sendFocCommand(uint8_t target_node_id, const VbdriveFocCommand& cmd) {
        validateNodeId(target_node_id);
        voltbro_foc_command_1_0 msg{};
        msg._torque.newton_meter = cmd.torque_Nm;
        msg.angle.radian = cmd.angle_rad;
        msg.velocity.radian_per_second = cmd.velocity_rad_s;
        msg.angle_kp.value = cmd.angle_kp;
        msg.velocity_kp.value = cmd.velocity_kp;
        msg.I_kp.value = cmd.current_kp;
        msg.I_ki.value = cmd.current_ki;
        interface_->send_msg(&msg,
                             static_cast<CanardPortID>(kVbdriveCommandBaseSubjectId + target_node_id),
                             &foc_command_tid_);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    void setMotorEnabled(uint8_t target_node_id, bool enabled) {
        validateNodeId(target_node_id);
        uavcan_register_Access_Request_1_0 request{};
        uavcan_register_Access_Request_1_0_initialize_(&request);
        constexpr char kRegisterName[] = "state.is_on";
        request.name.name.count = sizeof(kRegisterName) - 1U;
        for (size_t i = 0; i < request.name.name.count; ++i) {
            request.name.name.elements[i] = static_cast<uint8_t>(kRegisterName[i]);
        }
        uavcan_register_Value_1_0_select_bit_(&request.value);
        request.value.bit.value.count = 1;
        request.value.bit.value.bitpacked[0] = enabled ? 1U : 0U;

        interface_->send_request(&request,
                                 kRegisterAccessServiceId,
                                 &register_access_tid_,
                                 target_node_id);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    std::optional<VbdriveRegisterAccessResult> setMotorEnabledAndWait(
        uint8_t target_node_id,
        bool enabled,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        setMotorEnabled(target_node_id, enabled);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            interface_->loop();
            if (last_register_access_ && last_register_access_->source_node_id == target_node_id) {
                return last_register_access_;
            }
        }
        return std::nullopt;
    }

    void enableMotor(uint8_t target_node_id) { setMotorEnabled(target_node_id, true); }
    void disableMotor(uint8_t target_node_id) { setMotorEnabled(target_node_id, false); }
    void stopMotor(uint8_t target_node_id) { setVelocity(target_node_id, 0.0F); }

private:
    class StateSubscription : public AbstractSubscription<voltbro_foc_state_simple_1_0> {
    public:
        StateSubscription(VbdriveClient& owner, InterfacePtr& interface)
            : AbstractSubscription<voltbro_foc_state_simple_1_0>(interface, kVbdriveStateSimpleSubjectId),
              owner_(owner) {}

    private:
        void handler(const voltbro_foc_state_simple_1_0& msg, CanardRxTransfer* transfer) override {
            owner_.acceptState(msg, transfer);
        }

        VbdriveClient& owner_;
    };

    class RegisterAccessResponseSubscription
        : public AbstractSubscription<uavcan_register_Access_Response_1_0> {
    public:
        RegisterAccessResponseSubscription(VbdriveClient& owner, InterfacePtr& interface)
            : AbstractSubscription<uavcan_register_Access_Response_1_0>(
                  interface,
                  kRegisterAccessServiceId,
                  CanardTransferKindResponse),
              owner_(owner) {}

    private:
        void handler(const uavcan_register_Access_Response_1_0& msg, CanardRxTransfer* transfer) override {
            owner_.acceptRegisterAccessResponse(msg, transfer);
        }

        VbdriveClient& owner_;
    };

    static void validateNodeId(uint8_t node_id) {
        if (node_id == 0 || node_id > CANARD_NODE_ID_MAX) {
            throw ProtocolError("remote Cyphal node ID must be in range 1..127");
        }
    }

    void acceptState(const voltbro_foc_state_simple_1_0& msg, CanardRxTransfer* transfer) {
        last_state_ = convertVbdriveStateSimple(msg, transfer);
        stats_.state_messages++;
        if (state_cb_) state_cb_(*last_state_);
    }

    void acceptRegisterAccessResponse(const uavcan_register_Access_Response_1_0& msg,
                                      CanardRxTransfer* transfer) {
        VbdriveRegisterAccessResult out;
        if (transfer != nullptr) {
            out.source_node_id = static_cast<uint8_t>(transfer->metadata.remote_node_id);
            out.transfer_id = transfer->metadata.transfer_id;
        }
        out.mutable_register = msg._mutable;
        out.persistent_register = msg.persistent;
        if (uavcan_register_Value_1_0_is_bit_(&msg.value) && msg.value.bit.value.count > 0) {
            out.bit_value = (msg.value.bit.value.bitpacked[0] & 0x01U) != 0;
        }
        last_register_access_ = out;
    }

    CyphalInterfacePtr interface_;
    CanardTransferID specific_control_tid_{};
    CanardTransferID foc_command_tid_{};
    CanardTransferID register_access_tid_{};
    std::unique_ptr<StateSubscription> state_sub_;
    std::unique_ptr<RegisterAccessResponseSubscription> register_response_sub_;
    std::optional<VbdriveState> last_state_;
    std::optional<VbdriveRegisterAccessResult> last_register_access_;
    std::function<void(const VbdriveState&)> state_cb_;
    VbdriveStatistics stats_{};
};

}  // namespace voltbro::testbench
