# Developer Guide

Этот документ собирает в одном месте быстрый старт, архитектуру и минимальные
примеры использования `voltbro_testbench_client`.

## 1. Установка и первый запуск

### 1.1 Сначала нужен ethernet-can

Стенд работает через `VBCores/ethernet-can`:

```text
https://github.com/VBCores/ethernet-can
```

Этот проект не реализует UDP-протокол `ethernet-can` и не создает CAN-интерфейсы
самостоятельно. Сначала установите, настройте и запустите `ethernet-can`; именно
он создает Linux SocketCAN интерфейсы, с которыми работает клиент.

Текущая ожидаемая топология стенда:

```text
vcan1.0 -> VBDRIVE motor + DYN-200 telemetry + brake commands
```

Проверка перед запуском клиента:

```bash
ip link show vcan1.0
ip -details -statistics link show vcan1.0
candump vcan1.0
```

Если `vcan1.0` не существует, сначала разбирайтесь с `ethernet-can`, а не с этим
клиентом.

### 1.2 Зависимости клиента

- Ubuntu 24.04 или новее.
- CMake 3.22+.
- C++17 compiler.
- Linux SocketCAN headers.
- Опционально `can-utils` для `candump`.
- Доступ к GitHub при первой CMake-конфигурации, чтобы скачать pinned
  `VBCores/libcxxcanard` через FetchContent.
- Опционально Nunavut `nnvg`, только если нужно перегенерировать DSDL headers.

Обычная сборка не требует `nnvg`, потому что `generated/c` и `generated/cpp`
закоммичены в репозиторий.

### 1.3 Сборка с нуля

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

По умолчанию CMake скачает `libcxxcanard` в:

```text
build/_deps/libcxxcanard-src
build/_deps/libcxxcanard-build
```

Если сеть на целевой машине недоступна, заранее скачайте `libcxxcanard` и
передайте путь:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DVTC_LIBCXXCANARD_SOURCE_DIR=/path/to/libcxxcanard
cmake --build build -j
```

### 1.4 Первый запуск

Безопасные read-only проверки:

```bash
./build/testbench_discover --iface vcan1.0 --duration-ms 3000
./build/testbench_monitor --iface vcan1.0 --mode summary --duration 10
```

CSV-запись телеметрии:

```bash
./build/testbench_monitor \
  --iface vcan1.0 \
  --mode csv \
  --duration 30 \
  --motor-csv motor.csv \
  --dyn-csv dyn200_brake.csv
```

Примеры ниже отправляют команды и могут изменить состояние стенда:

```bash
./build/testbench_command --iface vcan1.0 motor --node-id 11 enable
./build/testbench_command --iface vcan1.0 motor --node-id 11 foc --torque 0 --angle 0 --velocity 10 --angle-kp 0 --velocity-kp 6 --i-kp 3 --i-ki 1300
./build/testbench_command --iface vcan1.0 motor --node-id 11 stop
./build/testbench_command --iface vcan1.0 brake off
./build/testbench_motor_sequence --iface vcan1.0 --node-id 11 --stage-ms 1500
./build/testbench_simple_csv
./build/testbench_direction_arrays
./build/testbench_torque_sweep --arm --min 0 --max 40 --step 1 --out torque_sweep.csv
.venv/bin/python tools/plot_torque_sweep.py torque_sweep.csv --out torque_sweep.png
```

Запускайте motor/brake examples только когда стенд физически безопасен.

## 2. Архитектура и основные абстракции

### 2.1 Что делает проект

`voltbro_testbench_client` - C++17 библиотека и набор примеров для PC-side
доступа к стенду через SocketCAN/Cyphal/CAN FD:

- VBDRIVE telemetry и команды.
- DYN-200 telemetry/status и команды.
- Brake command через subject `5103`.

### 2.2 Что проект не делает

Проект не реализует `ethernet-can` UDP data-plane и не содержит собственного
Cyphal/CAN transport для нормального RX/TX. Вся низкоуровневая Cyphal/CAN работа
делегирована `VBCores/libcxxcanard`.

### 2.3 Слои

- `VBCores/ethernet-can` создает SocketCAN интерфейсы.
- `VBCores/libcxxcanard` открывает SocketCAN, сериализует DSDL-типы, управляет
  transfer-id, tail byte, CAN FD DLC и подписками.
- `CyphalInterface` - объект шины из `libcxxcanard`.
- `VbdriveClient` - telemetry/commands мотора.
- `Dyn200Client` - DYN-200 state/status/commands.
- `BrakeClient` - brake commands.
- `TestbenchClient` - удобная обертка, которая может объединить все клиенты.

### 2.4 Файлы

- Public API: `include/voltbro_testbench_client/`.
- Примеры: `examples/`.
- DSDL definitions: `dsdl/`.
- Сгенерированные headers: `generated/c`, `generated/cpp`.
- Документация: `README.md`, `docs/`.

## 3. Минимальный пример: подключиться и прочитать сообщения

Ниже минимальный single-bus пример. Он открывает `vcan1.0`, создает клиентов и
читает несколько сообщений от каждого устройства.

```cpp
#include <voltbro_testbench_client/all.hpp>

#include <chrono>
#include <iostream>

int main() {
    using namespace voltbro::testbench;

    auto bus = makeCyphalInterface("vcan1.0", 100);
    VbdriveClient motor(bus);
    Dyn200Client dyn200(bus);

    unsigned motor_count = 0;
    unsigned dyn_state_count = 0;
    unsigned dyn_status_count = 0;

    motor.onState([&](const VbdriveState& s) {
        ++motor_count;
        std::cout << "motor vel=" << s.velocity_rad_s.value_or(0.0F)
                  << " torque=" << s.torque_Nm.value_or(0.0F) << "\n";
    });

    dyn200.onState([&](const Dyn200State& s) {
        ++dyn_state_count;
        std::cout << "dyn torque=" << s.torque_Nm << "\n";
    });

    dyn200.onStatus([&](const Dyn200Status& s) {
        ++dyn_status_count;
        std::cout << "dyn pub rate=" << s.cyphal_publication_rate_hz << "\n";
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline &&
           (motor_count < 3 || dyn_state_count < 3 || dyn_status_count < 1)) {
        bus->loop();
    }

    return 0;
}
```

Callback-и вызываются только внутри `bus->loop()`.

## 4. Отправка команд в topic/message

### 4.1 Команда мотору

FOC-команда публикуется как `voltbro.foc.command.1.0` на subject:

```text
2107 + node_id
```

Пример:

```cpp
VbdriveFocCommand foc;
foc.torque_Nm = 0.0F;
foc.angle_rad = 0.0F;
foc.velocity_rad_s = 10.0F;
foc.angle_kp = 0.0F;
foc.velocity_kp = 6.0F;
foc.current_kp = 3.0F;
foc.current_ki = 1300.0F;

motor.sendFocCommand(11, foc);
```

Удобные helpers:

```cpp
motor.setVelocity(11, 0.5F);
motor.setPosition(11, 1.0F);
motor.setTorque(11, 0.1F);
motor.setVoltage(11, 1.0F);
motor.stopMotor(11);
```

### 4.2 Команда DYN-200

DYN-200 команды публикуются на subject `5102` как
`voltbro.dynamometer.command.1.0`.

```cpp
dyn200.startAcquisition();
dyn200.setPublicationRate(50);
dyn200.readStatus();
dyn200.stopAcquisition();
```

`setRuntimeModbusAddress()` и `setRuntimeModbusBaudrate()` меняют runtime
параметры RAM-only; это не persistent configuration write.

```cpp
dyn200.setRuntimeModbusAddress(1);
dyn200.setRuntimeModbusBaudrate(38400);
```

`requestZero()` отправляет sensor zero request. Он должен вызываться только явно:

```cpp
dyn200.requestZero();
```

### 4.3 Команда тормозу

Brake command публикуется на subject `5103` как
`uavcan.primitive.scalar.Real32.1.0`.

```cpp
BrakeClient brake(bus);

brake.setRaw(0.25F);     // normalized 0.0..1.0
brake.setVoltage(2.5F);  // host-side 0.0..10.0 V helper
brake.disable();         // value = 0.0
```

У brake path нет прямого Cyphal service response.

## 5. Отправка команды в service мотору

Включение/выключение мотора идет не topic-message, а service request:

```text
uavcan.register.Access.1.0
service id: 384
register: state.is_on
```

Пример с ожиданием ответа:

```cpp
auto result = motor.setMotorEnabledAndWait(
    11,
    true,
    std::chrono::milliseconds(1000));

if (!result) {
    throw std::runtime_error("No state.is_on response from motor");
}
```

Отличие:

- topic/message: `send_msg()`, fire-and-forget, нет прямого ответа;
- service: `send_request()`, ожидается response от конкретного node-id.

## 6. Цикл обработки входящих сообщений

Главное правило: без вызова `bus->loop()` не принимаются кадры, не вызываются
callback-и и не обрабатываются service responses.

Типовой цикл:

```cpp
while (running) {
    bus->loop();
}
```

Для ограниченного окна:

```cpp
const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
while (std::chrono::steady_clock::now() < deadline) {
    bus->loop();
}
```

Рекомендации:

- не делайте тяжелую работу в callback;
- не печатайте каждую telemetry point при 1 kHz, лучше печатать последнюю точку
  раз в 100 ms;
- CSV/file output должен быть buffered;
- после отправки одноразовой команды можно вызвать `flushCyphalTx(bus)`, чтобы
  вытолкнуть накопленные TX-кадры.

## 7. Парсинг посылок и хранение

`libcxxcanard` передает в subscription handler уже десериализованный generated
DSDL struct. Клиент сразу конвертирует его в human-readable public struct:

- `convertVbdriveStateSimple()` -> `VbdriveState`;
- `convertDyn200State()` -> `Dyn200State`;
- `convertDyn200Status()` -> `Dyn200Status`.

### 7.1 Callback API

```cpp
motor.onState([](const VbdriveState& s) {
    // position, velocity, torque, current, voltage...
});

dyn200.onState([](const Dyn200State& s) {
    // angular velocity, torque, power...
});

dyn200.onStatus([](const Dyn200Status& s) {
    // error counters, publication rate...
});
```

### 7.2 Latest-value API

```cpp
auto motor_state = motor.lastState();
auto dyn_state = dyn200.lastState();
auto dyn_status = dyn200.lastStatus();
```

### 7.3 Хранение в массивы

`examples/testbench_direction_arrays.cpp` показывает простой подход:

- массив времени;
- массив положения мотора;
- массив скорости;
- массив момента.

Callback складывает каждую принятую telemetry point в активный набор массивов,
а основной цикл только меняет stage и периодически печатает последнюю точку.

### 7.4 CSV

`examples/testbench_simple_csv.cpp` показывает запись двух CSV без CLI:

- `simple_motor.csv`;
- `simple_dyn200.csv`.

Формат CSV описан в `docs/csv_format.md`.

`examples/testbench_torque_sweep.cpp` пишет отдельный CSV с усредненной
зависимостью motor torque telemetry и DYN-200 torque от заданного FOC torque.
Для построения PNG-графика используйте `tools/plot_torque_sweep.py`.
Скрипту нужен Python с Matplotlib; в рабочем окружении можно запускать его через
`.venv/bin/python`.

## 8. Полный пример: real-time управление и анализ

Ниже пример одного управляющего цикла: раз в секунду он отправляет мотору FOC
command с чередованием скорости `+10` / `-10 rad/s`, а между командами постоянно
вызывает `bus->loop()`, чтобы принимать telemetry и выталкивать TX-кадры.

Идея throttling такая:

- DYN-200 state обычно ниже по частоте, поэтому пример печатает каждое принятое
  DYN-200 сообщение;
- motor telemetry может идти часто, поэтому пример печатает каждое 100-е
  сообщение мотора;
- callbacks только обновляют счетчики и печатают короткую строку, тяжелый анализ
  лучше делать из накопленных данных или отдельного обработчика.

```cpp
#include <voltbro_testbench_client/all.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>

namespace {

constexpr const char* kIface = "vcan1.0";
constexpr uint8_t kLocalNodeId = 101;
constexpr uint8_t kMotorNodeId = 11;
constexpr float kVelocityRadS = 10.0F;

voltbro::testbench::VbdriveFocCommand makeVelocityCommand(float velocity_rad_s) {
    voltbro::testbench::VbdriveFocCommand cmd;
    cmd.torque_Nm = 0.0F;
    cmd.angle_rad = 0.0F;
    cmd.velocity_rad_s = velocity_rad_s;
    cmd.angle_kp = 0.0F;
    cmd.velocity_kp = 6.0F;
    cmd.current_kp = 3.0F;
    cmd.current_ki = 1300.0F;
    return cmd;
}

}  // namespace

int main() {
    using namespace voltbro::testbench;
    using Clock = std::chrono::steady_clock;

    auto bus = makeCyphalInterface(kIface, kLocalNodeId);
    VbdriveClient motor(bus);
    Dyn200Client dyn200(bus);

    uint64_t motor_rx = 0;
    uint64_t dyn_rx = 0;

    // Callback-и вызываются синхронно из bus->loop().
    // Здесь нельзя надолго блокироваться: иначе начнем терять входящие кадры.
    motor.onState([&](const VbdriveState& s) {
        ++motor_rx;

        if ((motor_rx % 100U) == 0U) {
            std::cout << "motor[" << motor_rx << "]"
                      << " src=" << unsigned(s.source_node_id)
                      << " pos=" << s.position_rad.value_or(0.0F)
                      << " vel=" << s.velocity_rad_s.value_or(0.0F)
                      << " torque=" << s.torque_Nm.value_or(0.0F)
                      << " current=" << s.current_A.value_or(0.0F)
                      << " fault=" << s.has_fault << "\n";
        }
    });

    dyn200.onState([&](const Dyn200State& s) {
        ++dyn_rx;

        std::cout << "dyn[" << dyn_rx << "]"
                  << " src=" << unsigned(s.source_node_id)
                  << " sample=" << s.sample_counter
                  << " speed=" << s.angular_velocity_rad_s
                  << " torque=" << s.torque_Nm
                  << " power=" << s.power_W
                  << " valid=" << s.sample_valid
                  << " flags=" << unsigned(s.status_flags) << "\n";
    });

    dyn200.onStatus([](const Dyn200Status& s) {
        std::cout << "dyn status:"
                  << " acq_hz=" << s.actual_acquisition_rate_hz
                  << " pub_hz=" << s.cyphal_publication_rate_hz
                  << " crc=" << s.crc_error_count
                  << " timeout=" << s.timeout_count << "\n";
    });

    std::cout << "Enable motor\n";
    auto enabled = motor.setMotorEnabledAndWait(
        kMotorNodeId,
        true,
        std::chrono::milliseconds(1000));
    if (!enabled) {
        std::cerr << "No state.is_on response from motor node "
                  << unsigned(kMotorNodeId) << "\n";
        return 1;
    }

    // Настраиваем DYN-200 на умеренную частоту публикации, чтобы cout не стал
    // бутылочным горлышком. Если firmware уже настроена, эти команды можно убрать.
    dyn200.setPublicationRate(50);
    dyn200.startAcquisition();

    const auto finish_at = Clock::now() + std::chrono::seconds(10);
    auto next_command_at = Clock::now();
    bool forward = true;

    while (Clock::now() < finish_at) {
        const auto now = Clock::now();

        if (now >= next_command_at) {
            const float velocity = forward ? kVelocityRadS : -kVelocityRadS;
            motor.sendFocCommand(kMotorNodeId, makeVelocityCommand(velocity));
            std::cout << "command: velocity=" << velocity << " rad/s\n";

            forward = !forward;
            next_command_at += std::chrono::seconds(1);
        }

        // Главная real-time часть: крутим loop как можно чаще.
        // Он принимает CAN frames, вызывает callbacks и обслуживает TX queue.
        bus->loop();
    }

    std::cout << "Stop motor\n";
    motor.stopMotor(kMotorNodeId);
    motor.disableMotor(kMotorNodeId);
    dyn200.stopAcquisition();
    flushCyphalTx(bus);

    std::cout << "Done: motor_rx=" << motor_rx << " dyn_rx=" << dyn_rx << "\n";
    return 0;
}
```

В реальном приложении вместо прямого `cout` из callback обычно делают одно из
двух: складывают свежие значения в lock-free/latest-value структуру и печатают
из основного цикла раз в `100 ms`, либо пишут в buffered CSV. Для демонстрации
выше `cout` оставлен прямо в callback, чтобы было видно, какие данные приходят
одновременно с управляющими командами.

## 9. Где смотреть готовые примеры

- `examples/testbench_discover.cpp` - read-only проверка устройств и протоколов.
- `examples/testbench_monitor.cpp` - summary/CSV мониторинг.
- `examples/testbench_command.cpp` - ручные команды motor/DYN-200/brake.
- `examples/testbench_simple_csv.cpp` - tutorial без CLI: команда мотору + два CSV.
- `examples/testbench_direction_arrays.cpp` - tutorial без CLI: вперед/назад и
  массивы telemetry.
- `examples/testbench_torque_sweep.cpp` - sweep FOC torque с усреднением
  motor/DYN-200 torque response в CSV.

## 10. Troubleshooting

- `vcan1.0` не существует: настройте и запустите `VBCores/ethernet-can`.
- `Permission denied` при открытии SocketCAN: проверьте capabilities/group
  permissions.
- Нет traffic: проверьте `candump vcan1.0` и конфигурацию bridge.
- FetchContent не скачивает `libcxxcanard`: дайте доступ к GitHub или используйте
  `VTC_LIBCXXCANARD_SOURCE_DIR`.
- `nnvg` не найден: он нужен только при `VTC_GENERATE_DSDL=ON`; обычная сборка
  использует committed generated headers.
