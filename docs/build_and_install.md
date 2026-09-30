# Сборка и подключение

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix ./install
```

Нужны Linux, CMake 3.22+ и компилятор C++17. `VBCores/libcxxcanard` закреплён на `6a8483b0271beeacda954b62de2c70e83d8f7fe3`. Если зависимость уже есть локально, добавьте `-DVTC_LIBCXXCANARD_SOURCE_DIR=/path/to/libcxxcanard`. Для отключения примеров используйте `-DVTC_BUILD_EXAMPLES=OFF`.

Сборка создаёт ровно четыре примера: `read_dyn200_raw`, `send_brake_5103`, `try_mit_motor` и `try_servo_motor`. Первые два используют libcxxcanard и сгенерированный DSDL `Real32` напрямую. Моторные примеры используют единственный собственный заголовок `include/voltbro_testbench_client/vbdrive.hpp` через интерфейсную CMake-цель `voltbro_testbench_client`. В `include/voltbro_testbench_client/` других заголовков нет.

Для приложения заказчика на DYN-200 ориентируйтесь на [минимальное руководство](raw_libcxxcanard_dyn200.md) и две исходные цели `read_dyn200_raw`/`send_brake_5103` в `CMakeLists.txt`. Подключение `voltbro_testbench_client` нужно только для локальных испытаний VBDRIVE.

Подключение VBDRIVE-клиента как подкаталога:

```cmake
add_subdirectory(path/to/pc_cyphal_client)
target_link_libraries(your_app PRIVATE voltbro_testbench_client)
```

Подключение VBDRIVE-клиента после установки:

```cmake
find_package(voltbro_testbench_client CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE voltbro::voltbro_testbench_client)
```

Исходники четырёх примеров находятся в `examples/`; каждый содержит `main()` без обработки аргументов. Параметры меняются в начале файла. Готовые DSDL-заголовки уже включены, поэтому `nnvg` для обычной сборки не нужен. Если нужно перегенерировать их, установите Nunavut `nnvg` и запустите `tools/generate_dsdl.sh`, указав `LIBCXXCANARD_DIR` при необходимости; альтернативно включите `-DVTC_GENERATE_DSDL=ON` и задайте `-DNNVG_EXECUTABLE=/path/to/nnvg`.
