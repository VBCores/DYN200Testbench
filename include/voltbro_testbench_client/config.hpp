#pragma once

#include <cstdint>
#include <string>

namespace voltbro::testbench {

struct ClientConfig {
    std::string motor_iface = "vcan1.0";
    std::string dyn_iface = "vcan1.0";
    uint8_t local_node_id = 100;
};

}  // namespace voltbro::testbench
