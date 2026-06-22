# Build And Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix ./install
```

The default build compiles a small static library plus examples. Public APIs are in `include/voltbro_testbench_client`.
The Cyphal/CAN runtime is provided by `VBCores/libcxxcanard`; by default CMake downloads it with FetchContent at pinned commit `07d2c2cd0e7719dc1321b1f91303e9f577e467b6`.
Use `-DVTC_LIBCXXCANARD_SOURCE_DIR=/path/to/libcxxcanard` to build against an existing local checkout.
Override with `-DVTC_LIBCXXCANARD_GIT_TAG=<commit>` only after validating the new upstream revision.

To regenerate Nunavut headers during the build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVTC_GENERATE_DSDL=ON -DNNVG_EXECUTABLE=/path/to/nnvg
cmake --build build -j
```

`tools/generate_dsdl.sh` generates both C headers under `generated/c` and C++ `libcxxcanard` traits under `generated/cpp`.

To consume from another CMake project without installation:

```cmake
add_subdirectory(path/to/pc_cyphal_client)
target_link_libraries(your_app PRIVATE voltbro_testbench_client)
```

To consume an installed package:

```cmake
find_package(voltbro_testbench_client CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE voltbro::voltbro_testbench_client)
```
