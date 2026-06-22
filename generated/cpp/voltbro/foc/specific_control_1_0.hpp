// This is an AUTO-GENERATED C++ traits header for libcxxcanard.
// Source file: /tmp/tmp.abIE5vZJqA/voltbro/foc/specific_control.1.0.dsdl
#pragma once

#include <cyphal/types.hpp>
#include <voltbro/foc/specific_control_1_0.h>

template <>
struct CyphalTypeTraits<voltbro_foc_specific_control_1_0> {
    using serializer_type = cyphal_serializer<voltbro_foc_specific_control_1_0>;
    using deserializer_type = cyphal_deserializer<voltbro_foc_specific_control_1_0>;

    static constexpr serializer_type serializer = voltbro_foc_specific_control_1_0_serialize_;
    static constexpr deserializer_type deserializer = voltbro_foc_specific_control_1_0_deserialize_;
    static constexpr size_t extent = voltbro_foc_specific_control_1_0_EXTENT_BYTES_;
    static constexpr size_t buffer_size = voltbro_foc_specific_control_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_;
};
