#include <voltbro_testbench_client/vbdrive.hpp>

#include <chrono>
#include <iostream>

using namespace voltbro::testbench;

int main() {
    // Стенд DYN-200 находится на vcan1.0, а сейчас подключённый мотор — на vcan2.0.
    // Если мотор перенесли, укажите его линию вплоть до vcan6.0.
    constexpr const char* can_interface = "vcan2.0";
    constexpr uint8_t local_node_id = 101;  // Должен быть свободным на выбранной линии.

    auto bus = makeCyphalInterface(can_interface, local_node_id);
    VbdriveClient motor(bus);  // Подписка на subject 3811, voltbro.foc.State.1.0.
    unsigned received = 0;
    unsigned printed = 0;
    motor.onState([&](const VbdriveState& state) {
        ++received;
        if (printed++ >= 10) return;  // Обычно около 1000 сообщений/с: не печатаем каждое.
        // На линии могут быть несколько моторов; source_node_id определяет отправителя.
        // State содержит только время, положение, скорость и момент. Остальные
        // измерения доступны через регистры, но не входят в это сообщение.
        std::cout << "Мотор " << unsigned(state.source_node_id)
                  << ": положение " << state.position_rad << " рад, скорость "
                  << state.velocity_rad_s << " рад/с, момент " << state.torque_Nm << " Н·м\n";
    });

    // Только чтение: пример не включает мотор и не отправляет уставки.
    // Колбэк выполняется из bus->loop(); свой цикл нужен для каждой линии.
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end) {
        bus->loop();
    }
    std::cout << "Получено сообщений VBDRIVE: " << received << '\n';
    return received ? 0 : 1;
}
