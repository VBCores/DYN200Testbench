cmake_minimum_required(VERSION 3.22)
project(voltbro_testbench_client VERSION 0.2.0 LANGUAGES CXX)

include(FetchContent)

option(VTC_BUILD_EXAMPLES "Build C++ examples" ON)
option(VTC_GENERATE_DSDL "Run Nunavut DSDL generation when nnvg is available" OFF)
set(NNVG_EXECUTABLE "" CACHE FILEPATH "Path to Nunavut nnvg executable")
set(VTC_LIBCXXCANARD_SOURCE_DIR ""
    CACHE PATH "Optional local libcxxcanard checkout. If empty or missing, FetchContent downloads VBCores/libcxxcanard.")
set(VTC_LIBCXXCANARD_GIT_TAG "6a8483b0271beeacda954b62de2c70e83d8f7fe3"
    CACHE STRING "Pinned VBCores/libcxxcanard commit used by FetchContent fallback.")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(VTC_LIBCXXCANARD_SOURCE_DIR AND EXISTS "${VTC_LIBCXXCANARD_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local libcxxcanard: ${VTC_LIBCXXCANARD_SOURCE_DIR}")
    add_subdirectory("${VTC_LIBCXXCANARD_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/libcxxcanard-build")
    set(VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR "${VTC_LIBCXXCANARD_SOURCE_DIR}")
else()
    FetchContent_Declare(
        libcxxcanard
        GIT_REPOSITORY https://github.com/VBCores/libcxxcanard.git
        GIT_TAG ${VTC_LIBCXXCANARD_GIT_TAG}
    )
    FetchContent_MakeAvailable(libcxxcanard)
    set(VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR "${libcxxcanard_SOURCE_DIR}")
endif()
target_include_directories(libcxxcanard PUBLIC
    $<BUILD_INTERFACE:${VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR}/libs>
    $<INSTALL_INTERFACE:include>
)

add_library(voltbro_testbench_client INTERFACE)
target_include_directories(voltbro_testbench_client
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated/c>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/voltbro_testbench_client_generated>
)
target_compile_features(voltbro_testbench_client INTERFACE cxx_std_17)
target_link_libraries(voltbro_testbench_client INTERFACE libcxxcanard)

if(VTC_GENERATE_DSDL)
    if(NOT NNVG_EXECUTABLE)
        find_program(NNVG_EXECUTABLE nnvg
            HINTS
                "${CMAKE_CURRENT_SOURCE_DIR}/../.venv/bin"
                "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin")
    endif()
    if(NNVG_EXECUTABLE)
        message(STATUS "nnvg found: ${NNVG_EXECUTABLE}")
        add_custom_target(vtc_generate_dsdl ALL
            COMMAND ${CMAKE_COMMAND} -E env
                "NNVG_EXECUTABLE=${NNVG_EXECUTABLE}"
                "LIBCXXCANARD_DIR=${VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dsdl.sh"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Generating Nunavut DSDL headers")
    else()
        message(FATAL_ERROR "VTC_GENERATE_DSDL=ON but nnvg was not found. Set -DNNVG_EXECUTABLE=/path/to/nnvg or activate the venv.")
    endif()
endif()

if(VTC_BUILD_EXAMPLES)
    foreach(example
        try_mit_motor
        try_servo_motor)
        add_executable(${example} examples/${example}.cpp)
        target_link_libraries(${example} PRIVATE voltbro_testbench_client)
        if(VTC_GENERATE_DSDL)
            add_dependencies(${example} vtc_generate_dsdl)
        endif()
    endforeach()

    # Raw example uses libcxxcanard directly and only generated DSDL type headers.
    add_executable(read_dyn200_raw examples/read_dyn200_raw.cpp)
    target_include_directories(read_dyn200_raw PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/c"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp")
    target_link_libraries(read_dyn200_raw PRIVATE libcxxcanard)

    add_executable(send_brake_5103 examples/send_brake_5103.cpp)
    target_include_directories(send_brake_5103 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/c"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp")
    target_link_libraries(send_brake_5103 PRIVATE libcxxcanard)
endif()

install(TARGETS voltbro_testbench_client libcxxcanard EXPORT voltbro_testbench_clientTargets)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY "${VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR}/cyphal" DESTINATION include)
install(DIRECTORY "${VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR}/libs/libcanard" DESTINATION include)
install(DIRECTORY "${VTC_LIBCXXCANARD_EFFECTIVE_SOURCE_DIR}/libs/o1heap" DESTINATION include)
install(DIRECTORY generated/c/ DESTINATION include/voltbro_testbench_client_generated)
install(DIRECTORY generated/cpp/ DESTINATION include/voltbro_testbench_client_generated)
install(EXPORT voltbro_testbench_clientTargets
    FILE voltbro_testbench_clientTargets.cmake
    NAMESPACE voltbro::
    DESTINATION lib/cmake/voltbro_testbench_client)
install(FILES cmake/voltbro_testbench_clientConfig.cmake
    DESTINATION lib/cmake/voltbro_testbench_client)
