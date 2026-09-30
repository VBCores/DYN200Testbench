#pragma once

#include <cyphal/allocators/sys/sys_allocator.h>
#include <cyphal/cyphal.h>
#include <cyphal/definitions.h>
#include <cyphal/providers/LinuxCAN.h>
#include <cyphal/subscriptions/subscription.h>
#include <libcanard/canard.h>
#include <uavcan/_register/Access_1_0.hpp>
#include <voltbro/foc/MIT_1_0.hpp>
#include <voltbro/foc/Servo_1_0.hpp>
#include <voltbro/foc/State_1_0.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace voltbro::testbench {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what) : std::runtime_error(what) {}
};

class ProtocolError : public Error {
public:
    explicit ProtocolError(const std::string& what) : Error(what) {}
};

constexpr uint16_t kVbdriveStateSubjectId = 3811;
constexpr uint16_t kVbdriveMitBaseSubjectId = 2107;
constexpr uint16_t kVbdriveServoBaseSubjectId = 3407;
constexpr uint16_t kRegisterAccessServiceId = 384;
constexpr size_t kDefaultTxQueueLength = 256;

using CyphalInterfacePtr = std::shared_ptr<CyphalInterface>;

inline CyphalInterfacePtr makeCyphalInterface(const std::string& ifname,
                                              uint8_t local_node_id,
                                              size_t tx_queue_len = kDefaultTxQueueLength) {
    if (local_node_id > CANARD_NODE_ID_MAX) {
        throw ProtocolError("local Cyphal node ID must be in range 0..127");
    }
    return CyphalInterface::create_heap<LinuxCAN, SystemAllocator>(
        local_node_id, ifname, tx_queue_len, DEFAULT_CONFIG);
}

inline void flushCyphalTx(const CyphalInterfacePtr& interface) {
    if (!interface) {
        throw ProtocolError("Cyphal interface is not initialized");
    }
    while (interface->has_unsent_frames()) {
        interface->process_tx_once();
    }
}

struct VbdriveStatistics {
    uint64_t state_messages{};
    uint64_t command_messages_sent{};
    uint64_t malformed_messages{};
    double state_rate_hz{};
};

struct VbdriveState {
    uint8_t source_node_id{};
    uint32_t transfer_id{};
    uint64_t timestamp_us{};
    float velocity_rad_s{};
    float position_rad{};
    float torque_Nm{};
    std::chrono::steady_clock::time_point host_receive_time{};
};

inline VbdriveState convertVbdriveState(const voltbro_foc_State_1_0& msg,
                                       const CanardRxTransfer* transfer) {
    VbdriveState s;
    if (transfer != nullptr) {
        s.source_node_id = static_cast<uint8_t>(transfer->metadata.remote_node_id);
        s.transfer_id = transfer->metadata.transfer_id;
    }
    s.timestamp_us = msg.timestamp.microsecond;
    s.position_rad = msg.pos.radian;
    s.velocity_rad_s = msg.vel.radian_per_second;
    s.torque_Nm = msg._torq.newton_meter;
    s.host_receive_time = std::chrono::steady_clock::now();
    return s;
}

enum class VbdriveServoMode : uint8_t {
    Velocity = voltbro_foc_Servo_1_0_VELOCITY,
    Torque = voltbro_foc_Servo_1_0_TORQUE,
    Position = voltbro_foc_Servo_1_0_POSITION,
    Voltage = voltbro_foc_Servo_1_0_VOLTAGE,
};

struct VbdriveMitCommand {
    float torque_Nm{};
    float position_rad{};
    float velocity_rad_s{};
    float position_gain{};
    float velocity_gain{};
};

struct VbdriveRegisterAccessResult {
    uint8_t source_node_id{};
    uint8_t transfer_id{};
    std::string name;
    bool mutable_register{};
    bool persistent_register{};
    uint8_t value_tag{};  // DSDL-variant tag; 0 означает пустое значение.
    std::optional<bool> bit_value;
    std::optional<int64_t> integer_value;
    std::optional<uint64_t> natural_value;
    std::optional<float> real32_value;
    std::optional<double> real64_value;
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

    void sendServoCommand(uint8_t target_node_id, VbdriveServoMode mode, float value) {
        validateNodeId(target_node_id);
        voltbro_foc_Servo_1_0 msg{};
        msg.set_point_type = static_cast<uint8_t>(mode);
        msg.set_point_value = value;
        interface_->send_msg(&msg,
                             static_cast<CanardPortID>(kVbdriveServoBaseSubjectId + target_node_id),
                             &servo_tid_);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    void setVelocity(uint8_t target_node_id, float rad_s) {
        sendServoCommand(target_node_id, VbdriveServoMode::Velocity, rad_s);
    }

    void setTorque(uint8_t target_node_id, float Nm) {
        sendServoCommand(target_node_id, VbdriveServoMode::Torque, Nm);
    }

    void setPosition(uint8_t target_node_id, float rad) {
        sendServoCommand(target_node_id, VbdriveServoMode::Position, rad);
    }

    void setVoltage(uint8_t target_node_id, float V) {
        sendServoCommand(target_node_id, VbdriveServoMode::Voltage, V);
    }

    void sendMitCommand(uint8_t target_node_id, const VbdriveMitCommand& cmd) {
        validateNodeId(target_node_id);
        voltbro_foc_MIT_1_0 msg{};
        msg._torq.newton_meter = cmd.torque_Nm;
        msg.pos.radian = cmd.position_rad;
        msg.vel.radian_per_second = cmd.velocity_rad_s;
        msg.pos_gain.value = cmd.position_gain;
        msg.vel_gain.value = cmd.velocity_gain;
        interface_->send_msg(&msg,
                             static_cast<CanardPortID>(kVbdriveMitBaseSubjectId + target_node_id),
                             &mit_tid_);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    void setMotorEnabled(uint8_t target_node_id, bool enabled) {
        validateNodeId(target_node_id);
        uavcan_register_Access_Request_1_0 request{};
        uavcan_register_Access_Request_1_0_initialize_(&request);
        setRegisterName(request, "is_on");
        uavcan_register_Value_1_0_select_bit_(&request.value);
        request.value.bit.value.count = 1;
        request.value.bit.value.bitpacked[0] = enabled ? 1U : 0U;

        pending_register_name_ = "is_on";
        last_register_access_.reset();
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

    void readRegister(uint8_t target_node_id, const std::string& name) {
        validateNodeId(target_node_id);
        uavcan_register_Access_Request_1_0 request{};
        uavcan_register_Access_Request_1_0_initialize_(&request);
        setRegisterName(request, name);
        uavcan_register_Value_1_0_select_empty_(&request.value);

        pending_register_name_ = name;
        last_register_access_.reset();
        interface_->send_request(&request,
                                 kRegisterAccessServiceId,
                                 &register_access_tid_,
                                 target_node_id);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    std::optional<VbdriveRegisterAccessResult> readRegisterAndWait(
        uint8_t target_node_id,
        const std::string& name,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        readRegister(target_node_id, name);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            interface_->loop();
            if (last_register_access_ &&
                last_register_access_->source_node_id == target_node_id &&
                last_register_access_->name == name) {
                return last_register_access_;
            }
        }
        return std::nullopt;
    }

    void enableMotor(uint8_t target_node_id) { setMotorEnabled(target_node_id, true); }
    void disableMotor(uint8_t target_node_id) { setMotorEnabled(target_node_id, false); }

private:
    class StateSubscription : public AbstractSubscription<voltbro_foc_State_1_0> {
    public:
        StateSubscription(VbdriveClient& owner, InterfacePtr& interface)
            : AbstractSubscription<voltbro_foc_State_1_0>(interface, kVbdriveStateSubjectId),
              owner_(owner) {}

    private:
        void handler(const voltbro_foc_State_1_0& msg, CanardRxTransfer* transfer) override {
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

    static void setRegisterName(uavcan_register_Access_Request_1_0& request, const std::string& name) {
        if (name.size() > uavcan_register_Name_1_0_name_ARRAY_CAPACITY_) {
            throw ProtocolError("register name is too long");
        }
        request.name.name.count = name.size();
        for (size_t i = 0; i < request.name.name.count; ++i) {
            request.name.name.elements[i] = static_cast<uint8_t>(name[i]);
        }
    }

    void acceptState(const voltbro_foc_State_1_0& msg, CanardRxTransfer* transfer) {
        last_state_ = convertVbdriveState(msg, transfer);
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
        out.name = pending_register_name_;
        out.mutable_register = msg._mutable;
        out.persistent_register = msg.persistent;
        out.value_tag = msg.value._tag_;
        if (uavcan_register_Value_1_0_is_bit_(&msg.value) && msg.value.bit.value.count > 0) {
            out.bit_value = (msg.value.bit.value.bitpacked[0] & 0x01U) != 0;
        }
        if (uavcan_register_Value_1_0_is_real32_(&msg.value) && msg.value.real32.value.count > 0) {
            out.real32_value = msg.value.real32.value.elements[0];
        }
        if (uavcan_register_Value_1_0_is_real64_(&msg.value) && msg.value.real64.value.count > 0) {
            out.real64_value = msg.value.real64.value.elements[0];
        }
        if (uavcan_register_Value_1_0_is_integer64_(&msg.value) && msg.value.integer64.value.count > 0) {
            out.integer_value = msg.value.integer64.value.elements[0];
        } else if (uavcan_register_Value_1_0_is_integer32_(&msg.value) && msg.value.integer32.value.count > 0) {
            out.integer_value = msg.value.integer32.value.elements[0];
        } else if (uavcan_register_Value_1_0_is_integer16_(&msg.value) && msg.value.integer16.value.count > 0) {
            out.integer_value = msg.value.integer16.value.elements[0];
        } else if (uavcan_register_Value_1_0_is_integer8_(&msg.value) && msg.value.integer8.value.count > 0) {
            out.integer_value = msg.value.integer8.value.elements[0];
        }
        if (uavcan_register_Value_1_0_is_natural64_(&msg.value) && msg.value.natural64.value.count > 0) {
            out.natural_value = msg.value.natural64.value.elements[0];
        } else if (uavcan_register_Value_1_0_is_natural32_(&msg.value) && msg.value.natural32.value.count > 0) {
            out.natural_value = msg.value.natural32.value.elements[0];
        } else if (uavcan_register_Value_1_0_is_natural16_(&msg.value) && msg.value.natural16.value.count > 0) {
            out.natural_value = msg.value.natural16.value.elements[0];
        } else if (uavcan_register_Value_1_0_is_natural8_(&msg.value) && msg.value.natural8.value.count > 0) {
            out.natural_value = msg.value.natural8.value.elements[0];
        }
        last_register_access_ = out;
    }

    CyphalInterfacePtr interface_;
    CanardTransferID servo_tid_{};
    CanardTransferID mit_tid_{};
    CanardTransferID register_access_tid_{};
    std::unique_ptr<StateSubscription> state_sub_;
    std::unique_ptr<RegisterAccessResponseSubscription> register_response_sub_;
    std::optional<VbdriveState> last_state_;
    std::optional<VbdriveRegisterAccessResult> last_register_access_;
    std::string pending_register_name_;
    std::function<void(const VbdriveState&)> state_cb_;
    VbdriveStatistics stats_{};
};

}  // namespace voltbro::testbench
