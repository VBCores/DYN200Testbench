#pragma once

#include "dyn200.hpp"
#include "time.hpp"
#include "vbdrive.hpp"

#include <fstream>
#include <string>

namespace voltbro::testbench {

class CsvLogger {
public:
    explicit CsvLogger(const std::string& path) : out_(path) {
        out_ << "host_time_ns,source,iface,source_node_id,subject_id,transfer_id,"
                "dyn_sample_counter,dyn_timestamp_us,dyn_angular_velocity_rad_s,dyn_torque_Nm,dyn_power_W,"
                "dyn_raw_speed,dyn_raw_torque,dyn_raw_power,dyn_status_flags,dyn_actual_acq_rate_hz,"
                "dyn_crc_error_count,dyn_timeout_count,motor_sample_counter,motor_timestamp_us,"
                "motor_position_rad,motor_velocity_rad_s,motor_torque_Nm,motor_current_A,motor_voltage_V,"
                "motor_temperature_C,motor_status_flags,parse_status,notes\n";
    }

    ~CsvLogger() { flush(); }

    void flush() { out_.flush(); }

    void writeDynState(const std::string& iface, const CyphalTransfer& tr, const Dyn200State& s) {
        out_ << steadyTimeNs(s.host_receive_time) << ",dyn200_state," << iface << "," << unsigned(tr.source_node_id)
             << "," << tr.subject_id << "," << unsigned(tr.transfer_id) << "," << s.sample_counter << ","
             << s.timestamp_us << "," << s.angular_velocity_rad_s << "," << s.torque_Nm << "," << s.power_W << ","
             << s.raw_speed << "," << s.raw_torque << "," << s.raw_power << "," << unsigned(s.status_flags)
             << ",,,,,,,,,,,,,ok,\n";
    }

    void writeDynStatus(const std::string& iface, const CyphalTransfer& tr, const Dyn200Status& s) {
        out_ << steadyTimeNs(s.host_receive_time) << ",dyn200_status," << iface << "," << unsigned(tr.source_node_id)
             << "," << tr.subject_id << "," << unsigned(tr.transfer_id) << "," << s.sample_counter
             << ",,,,,,,,,"
             << s.actual_acquisition_rate_hz << "," << s.crc_error_count << "," << s.timeout_count
             << ",,,,,,,,,,ok,\n";
    }

    void writeVbdriveState(const std::string& iface, const CyphalTransfer& tr, const VbdriveState& s) {
        out_ << steadyTimeNs(s.host_receive_time) << ",vbdrive_state," << iface << "," << unsigned(s.source_node_id)
             << "," << tr.subject_id << "," << unsigned(tr.transfer_id) << ",,,,,,,,,,,,,";
        out_ << "," << valueOrEmpty(s.timestamp_us) << "," << valueOrEmpty(s.position_rad) << ","
             << valueOrEmpty(s.velocity_rad_s) << "," << valueOrEmpty(s.torque_Nm) << ","
             << valueOrEmpty(s.current_A) << "," << valueOrEmpty(s.voltage_V) << ","
             << valueOrEmpty(s.temperature_C) << "," << (s.has_fault ? 1 : 0) << ",ok,\n";
    }

private:
    template <typename T>
    static std::string valueOrEmpty(const std::optional<T>& v) {
        return v ? std::to_string(*v) : std::string{};
    }

    std::ofstream out_;
};

}  // namespace voltbro::testbench
