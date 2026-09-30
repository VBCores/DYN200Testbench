#include <voltbro_testbench_client/dyn200.hpp>

#include <chrono>
#include <iostream>

using namespace voltbro::testbench;

int main() {
    // Контроллер стенда подключён к vcan1.0. Локальный ID ПК должен быть
    // свободным на этой линии и отличаться от ID контроллера (обычно 79).
    constexpr const char* can_interface = "vcan1.0";
    constexpr uint8_t local_node_id = 101;
    constexpr uint8_t dyn200_node_id = 79;

    auto bus = makeCyphalInterface(can_interface, local_node_id);
    Dyn200Client dyn200(bus, dyn200_node_id);  // Только подписки; команд нет.
    unsigned speed_count = 0;
    unsigned torque_count = 0;

    // 5100: Real32, скорость в об/мин. 5101: Real32, момент в Н·м.
    // Это два независимых потока, а не синхронная пара измерений.
    // Колбэки выполняются внутри bus->loop(), не в отдельном потоке.
    dyn200.onSpeed([&](const Dyn200Measurement& data) {
        if (++speed_count <= 5) std::cout << "Скорость: " << data.value << " об/мин\n";
    });
    dyn200.onTorque([&](const Dyn200Measurement& data) {
        if (++torque_count <= 5) std::cout << "Момент: " << data.value << " Н·м\n";
    });

    // В реальном приложении вызывайте loop() в своём основном/рабочем цикле.
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end) {
        bus->loop();
    }
    std::cout << "Получено: скорость " << speed_count << ", момент " << torque_count << '\n';
    return speed_count && torque_count ? 0 : 1;
}
