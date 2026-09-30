#include <voltbro_testbench_client/vbdrive.hpp>

#include <chrono>
#include <cmath>
#include <csignal>
#include <exception>
#include <iostream>
#include <optional>

using namespace voltbro::testbench;

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void requestStop(int) { stop_requested = 1; }
}  // namespace

int main() {
    // ФИЗИЧЕСКОЕ ДВИЖЕНИЕ: убедитесь, что механизм свободен и доступен аварийный стоп.
    // 0.02 рад/с ~= 0.19 об/мин; за 10 с ожидается около 0.2 рад (11.5°).
    constexpr const char* can_interface = "vcan2.0";
    constexpr uint8_t local_node_id = 101;
    constexpr uint8_t motor_node_id = 4;
    constexpr float speed_rad_s = 0.02F;
    constexpr auto run_time = std::chrono::seconds(10);
    constexpr bool supervised_nan_limits_override = false;  // Разовый тест завершён; защита включена.

    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    auto bus = makeCyphalInterface(can_interface, local_node_id);
    VbdriveClient motor(bus);

    // Двигатель должен быть уже включён, но стоять. Не меняем is_on и
    // не перехватываем мотор, которым в данный момент управляет другой клиент.
    const auto on = motor.readRegisterAndWait(motor_node_id, "is_on");
    if (!on || !on->bit_value.value_or(false)) {
        std::cerr << "is_on не равен 1; движение отменено\n";
        return 1;
    }

    // Проверяем фактические настройки привода до отправки уставки.
    const auto readReal = [&](const char* name, bool& unset) -> std::optional<float> {
        const auto result = motor.readRegisterAndWait(motor_node_id, name);
        if (!result) {
            std::cerr << "Нет ответа на запрос регистра " << name << '\n';
            return std::nullopt;
        }
        const auto value = result->real32_value ? result->real32_value
            : (result->real64_value ? std::optional<float>(*result->real64_value) : std::nullopt);
        if (!value) {
            std::cerr << "Нет числового значения регистра " << name
                      << " (тип DSDL " << unsigned(result->value_tag) << ")\n";
            return std::nullopt;
        }
        if (!std::isfinite(*value)) {
            unset = std::isnan(*value);
            std::cerr << "Регистр " << name << " равен NaN/Inf: безопасный лимит не задан\n";
            return std::nullopt;
        }
        std::cout << name << " = " << *value << '\n';
        return value;
    };
    bool speed_unset = false, current_unset = false, torque_unset = false;
    const auto max_speed = readReal("max_spd", speed_unset);
    const auto max_current = readReal("max_i", current_unset);
    const auto max_torque = readReal("max_tq", torque_unset);
    const bool limits_safe = max_speed && max_current && max_torque &&
        *max_speed >= speed_rad_s && *max_current > 0.0F && *max_current <= 0.5F &&
        *max_torque > 0.0F && *max_torque <= 5.0F;
    const bool supervised_override = supervised_nan_limits_override &&
        speed_unset && current_unset && torque_unset;
    if (!limits_safe && !supervised_override) {
        std::cerr << "Ограничения мотора не прошли консервативную проверку; движение отменено\n";
        return 1;
    }
    if (supervised_override) {
        std::cerr << "ВНИМАНИЕ: три лимита NaN; разовый тест под контролем оператора\n";
    }

    VbdriveState state{};
    bool have_state = false;
    motor.onState([&](const VbdriveState& value) {
        if (value.source_node_id == motor_node_id) {
            state = value;
            have_state = true;
        }
    });
    const auto preflight_end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!have_state && std::chrono::steady_clock::now() < preflight_end) bus->loop();
    if (!have_state || std::abs(state.velocity_rad_s) > 0.1F || stop_requested) {
        std::cerr << "Нет телеметрии или мотор уже движется; движение отменено\n";
        return 1;
    }
    const float start_position = state.position_rad;
    std::cout << "Старт: положение " << start_position << " рад, скорость "
              << state.velocity_rad_s << " рад/с\n";

    bool command_attempted = false;
    bool completed = false;
    bool safety_abort = false;
    try {
        command_attempted = true;
        motor.setVelocity(motor_node_id, speed_rad_s);  // Servo subject 3407 + node ID.
        const auto end = std::chrono::steady_clock::now() + run_time;
        while (!stop_requested && std::chrono::steady_clock::now() < end) {
            bus->loop();  // Обрабатывает телеметрию и исходящие кадры.
            if (!have_state ||
                std::chrono::steady_clock::now() - state.host_receive_time > std::chrono::milliseconds(500) ||
                std::abs(state.velocity_rad_s) > 0.3F ||
                std::abs(state.position_rad - start_position) > 0.35F) {
                std::cerr << "Потеря телеметрии или превышение порога движения\n";
                safety_abort = true;
                break;
            }
        }
        completed = !safety_abort && !stop_requested && std::chrono::steady_clock::now() >= end;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка во время движения: " << e.what() << '\n';
    }

    // Уставка удерживается, поэтому ноль отправляем при любом штатном исходе
    // после попытки движения. SIGINT/SIGTERM тоже приводят сюда; SIGKILL — нет.
    if (command_attempted) {
        try {
            motor.setVelocity(motor_node_id, 0.0F);
            std::cout << "Отправлена нулевая уставка. Итог: положение " << state.position_rad
                      << " рад, скорость " << state.velocity_rad_s << " рад/с\n";
        } catch (const std::exception& e) {
            std::cerr << "ВНИМАНИЕ: не удалось отправить ноль: " << e.what() << '\n';
            return 1;
        }
    }
    return completed ? 0 : 1;
}
