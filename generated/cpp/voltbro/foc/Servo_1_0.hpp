// This is an AUTO-GENERATED C++ traits header for libcxxcanard.
// Source file: /srv/codex-work/dyn200cxxclient/pc_cyphal_client/dsdl/voltbro_types/voltbro/foc/Servo.1.0.dsdl
#pragma once

#include <cyphal/types.hpp>
#include <voltbro/foc/Servo_1_0.h>

template <>
struct CyphalTypeTraits<voltbro_foc_Servo_1_0> {
    using serializer_type = cyphal_serializer<voltbro_foc_Servo_1_0>;
    using deserializer_type = cyphal_deserializer<voltbro_foc_Servo_1_0>;

    static constexpr serializer_type serializer = voltbro_foc_Servo_1_0_serialize_;
    static constexpr deserializer_type deserializer = voltbro_foc_Servo_1_0_deserialize_;
    static constexpr size_t extent = voltbro_foc_Servo_1_0_EXTENT_BYTES_;
    static constexpr size_t buffer_size = voltbro_foc_Servo_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_;
};
