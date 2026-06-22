#pragma once

#include "cyphal_node.hpp"
#include "statistics.hpp"

#include <cyphal/subscriptions/subscription.h>
#include <voltbro/dynamometer/command_1_0.hpp>
#include <voltbro/dynamometer/state_1_0.hpp>
#include <voltbro/dynamometer/status_1_0.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace voltbro::testbench {

struct Dyn200State {
    uint8_t source_node_id{};
    uint8_t transfer_id{};
    uint32_t sample_counter{};
    uint64_t timestamp_us{};
    float angular_velocity_rad_s{};
    float torque_Nm{};
    float power_W{};
    int32_t raw_speed{};
    int32_t raw_torque{};
    int32_t raw_power{};
    uint8_t status_flags{};
    bool sample_valid{};
    bool scaling_valid{};
    bool repeated_sample{};
    bool modbus_timeout_recent{};
    bool modbus_crc_error_recent{};
    bool sensor_config_uncertain{};
    bool brake_stub_active{};
    std::chrono::steady_clock::time_point host_receive_time{};
};

struct Dyn200Status {
    uint8_t source_node_id{};
    uint8_t transfer_id{};
    uint32_t sample_counter{};
    uint32_t crc_error_count{};
    uint32_t timeout_count{};
    uint32_t frame_sync_count{};
    uint32_t uart_error_count{};
    uint32_t cyphal_tx_error_count{};
    uint32_t cyphal_rx_error_count{};
    uint32_t same_sample_republished_count{};
    float actual_acquisition_rate_hz{};
    float cyphal_publication_rate_hz{};
    uint8_t dyn200_address{};
    uint32_t dyn200_baudrate{};
    uint8_t last_status{};
    std::chrono::steady_clock::time_point host_receive_time{};
};

inline Dyn200State convertDyn200State(const voltbro_dynamometer_state_1_0& msg,
                                      const CanardRxTransfer* transfer) {
    Dyn200State s;
    if (transfer != nullptr) {
        s.source_node_id = static_cast<uint8_t>(transfer->metadata.remote_node_id);
        s.transfer_id = transfer->metadata.transfer_id;
    }
    s.sample_counter = msg.sample_counter;
    s.timestamp_us = msg.timestamp_us;
    s.angular_velocity_rad_s = msg.angular_velocity.radian_per_second;
    s.torque_Nm = msg._torque.newton_meter;
    s.power_W = msg.power.watt;
    s.raw_speed = msg.raw_speed;
    s.raw_torque = msg.raw_torque;
    s.raw_power = msg.raw_power;
    s.status_flags = msg.status_flags;
    s.sample_valid = (s.status_flags & (1U << 0U)) != 0;
    s.scaling_valid = (s.status_flags & (1U << 1U)) != 0;
    s.repeated_sample = (s.status_flags & (1U << 2U)) != 0;
    s.modbus_timeout_recent = (s.status_flags & (1U << 3U)) != 0;
    s.modbus_crc_error_recent = (s.status_flags & (1U << 4U)) != 0;
    s.sensor_config_uncertain = (s.status_flags & (1U << 5U)) != 0;
    s.brake_stub_active = (s.status_flags & (1U << 6U)) != 0;
    s.host_receive_time = std::chrono::steady_clock::now();
    return s;
}

inline Dyn200Status convertDyn200Status(const voltbro_dynamometer_status_1_0& msg,
                                        const CanardRxTransfer* transfer) {
    Dyn200Status s;
    if (transfer != nullptr) {
        s.source_node_id = static_cast<uint8_t>(transfer->metadata.remote_node_id);
        s.transfer_id = transfer->metadata.transfer_id;
    }
    s.sample_counter = msg.sample_counter;
    s.crc_error_count = msg.crc_error_count;
    s.timeout_count = msg.timeout_count;
    s.frame_sync_count = msg.frame_sync_count;
    s.uart_error_count = msg.uart_error_count;
    s.cyphal_tx_error_count = msg.cyphal_tx_error_count;
    s.cyphal_rx_error_count = msg.cyphal_rx_error_count;
    s.same_sample_republished_count = msg.same_sample_republished_count;
    s.actual_acquisition_rate_hz = msg.actual_acquisition_rate.value;
    s.cyphal_publication_rate_hz = msg.cyphal_publication_rate.value;
    s.dyn200_address = msg.dyn200_address;
    s.dyn200_baudrate = msg.dyn200_baudrate;
    s.last_status = msg.last_status;
    s.host_receive_time = std::chrono::steady_clock::now();
    return s;
}

class Dyn200Client {
public:
    explicit Dyn200Client(CyphalInterfacePtr interface)
        : interface_(std::move(interface)),
          state_sub_(std::make_unique<StateSubscription>(*this, interface_)),
          status_sub_(std::make_unique<StatusSubscription>(*this, interface_)) {}

    void onState(std::function<void(const Dyn200State&)> cb) { state_cb_ = std::move(cb); }
    void onStatus(std::function<void(const Dyn200Status&)> cb) { status_cb_ = std::move(cb); }
    std::optional<Dyn200State> lastState() const { return last_state_; }
    std::optional<Dyn200Status> lastStatus() const { return last_status_; }
    Dyn200Statistics statistics() const { return stats_; }

    void startAcquisition() { sendCommand(voltbro_dynamometer_command_1_0_START_ACQUISITION, 0, 0.0F); }
    void stopAcquisition() { sendCommand(voltbro_dynamometer_command_1_0_STOP_ACQUISITION, 0, 0.0F); }
    void setAcquisitionRate(uint32_t hz) {
        sendCommand(voltbro_dynamometer_command_1_0_SET_ACQUISITION_RATE, hz, static_cast<float>(hz));
    }
    void setPublicationRate(uint32_t hz) {
        sendCommand(voltbro_dynamometer_command_1_0_SET_PUBLICATION_RATE, hz, static_cast<float>(hz));
    }
    void requestZero() { sendCommand(voltbro_dynamometer_command_1_0_ZERO_REQUEST, 0, 0.0F); }
    void readStatus() { sendCommand(voltbro_dynamometer_command_1_0_READ_STATUS, 0, 0.0F); }
    void setRuntimeModbusAddress(uint8_t address) {
        sendCommand(voltbro_dynamometer_command_1_0_SET_MODBUS_ADDRESS_RAM_ONLY, address, 0.0F);
    }
    void setRuntimeModbusBaudrate(uint32_t baudrate) {
        if (baudrate != 9600 && baudrate != 14400 && baudrate != 19200 && baudrate != 38400) {
            throw ProtocolError("invalid DYN-200 RAM-only baudrate");
        }
        sendCommand(voltbro_dynamometer_command_1_0_SET_BAUD_RAM_ONLY, baudrate, 0.0F);
    }

private:
    class StateSubscription : public AbstractSubscription<voltbro_dynamometer_state_1_0> {
    public:
        StateSubscription(Dyn200Client& owner, InterfacePtr& interface)
            : AbstractSubscription<voltbro_dynamometer_state_1_0>(interface, kDyn200StateSubjectId),
              owner_(owner) {}

    private:
        void handler(const voltbro_dynamometer_state_1_0& msg, CanardRxTransfer* transfer) override {
            owner_.acceptState(msg, transfer);
        }

        Dyn200Client& owner_;
    };

    class StatusSubscription : public AbstractSubscription<voltbro_dynamometer_status_1_0> {
    public:
        StatusSubscription(Dyn200Client& owner, InterfacePtr& interface)
            : AbstractSubscription<voltbro_dynamometer_status_1_0>(interface, kDyn200StatusSubjectId),
              owner_(owner) {}

    private:
        void handler(const voltbro_dynamometer_status_1_0& msg, CanardRxTransfer* transfer) override {
            owner_.acceptStatus(msg, transfer);
        }

        Dyn200Client& owner_;
    };

    void acceptState(const voltbro_dynamometer_state_1_0& msg, CanardRxTransfer* transfer) {
        last_state_ = convertDyn200State(msg, transfer);
        stats_.state_messages++;
        if (state_cb_) state_cb_(*last_state_);
    }

    void acceptStatus(const voltbro_dynamometer_status_1_0& msg, CanardRxTransfer* transfer) {
        last_status_ = convertDyn200Status(msg, transfer);
        stats_.status_messages++;
        if (status_cb_) status_cb_(*last_status_);
    }

    void sendCommand(uint8_t command, uint32_t argument_u32, float argument_real32) {
        voltbro_dynamometer_command_1_0 msg{};
        msg.command = command;
        msg.argument_u32 = argument_u32;
        msg.argument_real32.value = argument_real32;
        interface_->send_msg(&msg, kDyn200CommandSubjectId, &dyn_command_tid_);
        flushCyphalTx(interface_);
        stats_.command_messages_sent++;
    }

    CyphalInterfacePtr interface_;
    CanardTransferID dyn_command_tid_{};
    std::unique_ptr<StateSubscription> state_sub_;
    std::unique_ptr<StatusSubscription> status_sub_;
    std::optional<Dyn200State> last_state_;
    std::optional<Dyn200Status> last_status_;
    std::function<void(const Dyn200State&)> state_cb_;
    std::function<void(const Dyn200Status&)> status_cb_;
    Dyn200Statistics stats_{};
};

}  // namespace voltbro::testbench
