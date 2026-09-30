#include <voltbro_testbench_client/vbdrive.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace voltbro::testbench;

int main() {
    // Настройки этого примера: мотор 4 на vcan1.0, ПК занимает свободный ID 101.
    // Пример двигает РЕАЛЬНЫЙ мотор. Перед запуском проверьте механику и питание.
    constexpr uint8_t motor_id = 4;
    auto bus = makeCyphalInterface("vcan1.0", 101);
    VbdriveClient motor(bus);

    try {
        // is_on=1 через uavcan.register.Access (сервис 384).
        // Ждём ответа: без подтверждения нельзя считать, что драйвер включён.
        const auto on = motor.setMotorEnabledAndWait(motor_id, true);
        if (!on || !on->bit_value.value_or(false)) {
            motor.disableMotor(motor_id);  // Запрос мог дойти, даже если ответ потерялся.
            std::cerr << "Включение не подтверждено; MIT-команда не отправлена. "
                         "Проверьте is_on и перед повтором выждите 10 секунд после выключения\n";
            return 1;
        }

        // Поля называются иначе и идут в другом порядке, чем аргументы UART.
        // Получается точный эквивалент: mit_cmd: 0 1 0 0 0.5
        VbdriveMitCommand cmd{};
        cmd.position_rad = 0.0F;     // pos: положение, рад; усиление ниже равно 0.
        cmd.velocity_rad_s = 1.0F;   // vel: целевая скорость, рад/с.
        cmd.torque_Nm = 0.0F;        // torq: добавочный момент, Н·м.
        cmd.position_gain = 0.0F;    // p_gain: позиционная составляющая выключена.
        cmd.velocity_gain = 0.5F;    // v_gain: коэффициент скорости.
        motor.sendMitCommand(motor_id, cmd);  // Cyphal subject 2107 + 4 = 2111.

        // Одна команда удерживается контроллером; через 5 секунд останавливаем.
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < end) bus->loop();

        motor.sendMitCommand(motor_id, {});  // Все пять полей = 0: нулевое усилие.
        const auto off = motor.setMotorEnabledAndWait(motor_id, false);  // is_on=0.
        if (!off || !off->bit_value || *off->bit_value) {
            motor.disableMotor(motor_id);  // Повторяем, если ответ не подтвердил выключение.
            std::cerr << "Выключение не подтверждено — проверьте мотор! "
                         "После фактического выключения выждите 10 секунд перед повтором\n";
            return 1;
        }
        std::cout << "Нулевая MIT-команда отправлена; is_on=0 подтверждено\n";
        // В текущей прошивке есть подтверждённый разработчиком баг: сразу после
        // выключения повторное is_on=1 может не сработать. Не оставляем мотор
        // включённым ради обхода бага; выдерживаем паузу перед следующим запуском.
        std::cout << "Пауза 10 секунд после is_on=0 из-за бага прошивки..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "Пауза завершена; пример можно запускать снова\n";
    } catch (...) {
        // При обычном исключении пытаемся снять питание с драйвера.
        // Stop в отладчике, SIGKILL и потеря связи этот код НЕ выполняют:
        // для реального стенда нужен аппаратный способ обесточивания.
        try { motor.disableMotor(motor_id); } catch (...) {}
        throw;
    }
}
