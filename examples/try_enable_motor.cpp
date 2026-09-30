#include <voltbro_testbench_client/vbdrive.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

using namespace voltbro::testbench;

int main() {
    // ВНИМАНИЕ: пример включает физический драйвер мотора на две секунды.
    // Запускайте только если механика свободна и никто другой не посылает уставки.
    constexpr const char* can_interface = "vcan2.0";
    constexpr uint8_t local_node_id = 101;  // Свободный ID ПК на этой линии.
    constexpr uint8_t motor_node_id = 4;     // Проверен на текущем vcan2.0; у вас может быть другой.

    auto bus = makeCyphalInterface(can_interface, local_node_id);
    VbdriveClient motor(bus);


    // Не трогаем уже включённый привод и не рискуем, если регистр недоступен.
    const auto before = motor.readRegisterAndWait(motor_node_id, "is_on");
    if (!before || !before->bit_value) {
        std::cerr << "Не удалось прочитать бит is_on; включение отменено\n";
        return 1;
    }
    if (*before->bit_value) {
        std::cerr << "Мотор уже включён; состояние не менялось\n";
        return 1;
    }

    // Флаг ставится до отправки: даже при исключении после передачи мы
    // попытаемся выключить драйвер. Это не заменяет аппаратный аварийный стоп.
    bool activation_attempted = false;
    bool enabled_confirmed = false;
    try {
        activation_attempted = true;
        const auto enabled = motor.setMotorEnabledAndWait(motor_node_id, true);
        enabled_confirmed = enabled && enabled->bit_value.value_or(false);
        std::cout << "Подтверждение включения: " << (enabled_confirmed ? "да" : "нет") << '\n';

        // Команду скорости/момента/положения не отправляем. По текущей прошивке
        // после включения драйвер ждёт новую уставку с нулевым усилием.
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (enabled_confirmed && std::chrono::steady_clock::now() < end) {
            bus->loop();
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка проверки: " << e.what() << '\n';
    }

    if (activation_attempted) {
        // Подтверждаем выключение отдельным запросом, даже если включение не
        // подтвердилось. При потере связи нужен аппаратный аварийный стоп.
        try {
            const auto disabled = motor.setMotorEnabledAndWait(motor_node_id, false);
            if (!disabled || !disabled->bit_value || *disabled->bit_value) {
                std::cerr << "ВНИМАНИЕ: выключение не подтверждено!\n";
                return 1;
            }
            std::cout << "Выключение подтверждено\n";
            // Обход известного бага прошивки: слишком быстрое повторное
            // is_on=1 после is_on=0 может быть отвергнуто контроллером.
            std::cout << "Пауза 10 секунд после is_on=0 из-за бага прошивки..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::cout << "Пауза завершена; пример можно запускать снова\n";
        } catch (const std::exception& e) {
            std::cerr << "ВНИМАНИЕ: ошибка при выключении: " << e.what() << '\n';
            return 1;
        }
    }
    return enabled_confirmed ? 0 : 1;
}
