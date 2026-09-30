#include <voltbro_testbench_client/brake.hpp>

#include <chrono>
#include <iostream>

using namespace voltbro::testbench;

int main() {
    // Этот пример физически управляет тормозом на vcan1.0. Проверьте нагрузку.
    // Контроллер стенда обычно имеет ID 79; ID ПК должен быть свободным.
    constexpr const char* can_interface = "vcan1.0";
    constexpr uint8_t local_node_id = 101;
    // Subject 5103: Real32 от 0 (выключено) до 1 (полная уставка).
    // По умолчанию безопасный ноль; ненулевую уставку задавайте осознанно.
    constexpr float brake_command = 0.0F;

    auto bus = makeCyphalInterface(can_interface, local_node_id);
    BrakeClient brake(bus);
    brake.setNormalized(brake_command);  // Публикация уставки; подтверждения нет.

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    try {
        while (std::chrono::steady_clock::now() < end) {
            bus->loop();
        }
    } catch (...) {
        brake.disable();
        throw;
    }
    // Прошивка удерживает последнюю команду без тайм-аута. При штатном выходе
    // и C++ исключении публикуем ноль. После аварийного завершения процесса
    // автоматического сброса нет: это нужно учесть в безопасной системе.
    brake.disable();
    std::cout << "Опубликована команда тормоза " << brake_command
              << " и затем команда сброса в ноль\n";
}
