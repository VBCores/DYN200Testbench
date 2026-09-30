#include <voltbro_testbench_client/vbdrive.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <exception>
#include <iostream>
#include <initializer_list>
#include <thread>

using namespace voltbro::testbench;

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void requestStop(int) { stop_requested = 1; }
}  // namespace

int main() {
    // ФИЗИЧЕСКОЕ ДВИЖЕНИЕ: оператор должен контролировать мотор и питание.
    // Cyphal Servo VELOCITY соответствует UART `servo_cmd: 0 <speed>`.
    constexpr const char* can_interface = "vcan1.0";
    constexpr uint8_t local_node_id = 101;
    constexpr uint8_t motor_node_id = 4;
    constexpr float speed_rad_s = 1.0F;  // Эквивалент UART `servo_cmd: 0 1`.
    constexpr auto command_duration = std::chrono::seconds(2);

    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    auto bus = makeCyphalInterface(can_interface, local_node_id);
    VbdriveClient motor(bus);

    // Читаем состояние до пробы. Если драйвер уже включён оператором,
    // повторное включение не требуется.
    const auto on = motor.readRegisterAndWait(motor_node_id, "is_on");
    if (!on || !on->bit_value) {
        std::cerr << "Не удалось прочитать is_on; проба отменена\n";
        return 1;
    }
    std::cout << "is_on = " << *on->bit_value << '\n';
    for (const char* name : {"servo_vel_p_gain", "servo_vel_i_gain", "max_i", "max_tq", "max_spd"}) {
        const auto gain = motor.readRegisterAndWait(motor_node_id, name);
        if (gain && gain->real32_value) {
            std::cout << name << " = " << *gain->real32_value << '\n';
        }
    }

    VbdriveState state{};
    bool have_state = false;
    unsigned samples = 0;
    unsigned overspeed_streak = 0;
    double velocity_sum = 0.0;
    double velocity_square_sum = 0.0;
    float min_velocity = 0.0F;
    float max_velocity = 0.0F;
    motor.onState([&](const VbdriveState& sample) {
        if (sample.source_node_id != motor_node_id) return;
        state = sample;
        have_state = true;
        ++samples;
        velocity_sum += sample.velocity_rad_s;
        velocity_square_sum += double(sample.velocity_rad_s) * sample.velocity_rad_s;
        if (samples == 1 || sample.velocity_rad_s < min_velocity) min_velocity = sample.velocity_rad_s;
        if (samples == 1 || sample.velocity_rad_s > max_velocity) max_velocity = sample.velocity_rad_s;
        overspeed_streak = std::abs(sample.velocity_rad_s) > 3.0F ? overspeed_streak + 1 : 0;
    });
    const auto preflight_end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!have_state && std::chrono::steady_clock::now() < preflight_end) bus->loop();
    if (!have_state || std::abs(state.velocity_rad_s) > 0.3F || stop_requested) {
        std::cerr << "Нет телеметрии или мотор уже движется; проба отменена\n";
        return 1;
    }

    bool enable_attempted = false;
    bool command_attempted = false;
    bool completed = false;
    const float start_position = state.position_rad;
    try {
        bool ready = *on->bit_value;
        if (!ready) {
            enable_attempted = true;
            const auto enabled = motor.setMotorEnabledAndWait(
                motor_node_id, true, std::chrono::milliseconds(1000));
            ready = enabled && enabled->bit_value.value_or(false);
            if (!ready) std::cerr << "Включение мотора не подтверждено\n";
        }
        if (ready) {
            samples = 0;
            velocity_sum = 0.0;
            velocity_square_sum = 0.0;
            overspeed_streak = 0;
            command_attempted = true;
            motor.setVelocity(motor_node_id, speed_rad_s);  // Servo subject 3407 + 4 = 3411.
            const auto end = std::chrono::steady_clock::now() + command_duration;
            bool aborted = false;
            while (!stop_requested && std::chrono::steady_clock::now() < end) {
                bus->loop();
                if (overspeed_streak >= 20 ||
                    std::chrono::steady_clock::now() - state.host_receive_time > std::chrono::milliseconds(500) ||
                    std::abs(state.position_rad - start_position) > 3.0F) {
                    std::cerr << "Превышение порога движения или потеря телеметрии\n";
                    aborted = true;
                    break;
                }
            }
            completed = !aborted && !stop_requested && std::chrono::steady_clock::now() >= end;
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка Servo-пробы: " << e.what() << '\n';
    }

    if (command_attempted) {
        try {
            motor.setVelocity(motor_node_id, 0.0F);
        } catch (const std::exception& e) {
            std::cerr << "ВНИМАНИЕ: не удалось отправить нулевую скорость: " << e.what() << '\n';
            completed = false;
        }
    }
    if (command_attempted || enable_attempted) {
        try {
            const auto disabled = motor.setMotorEnabledAndWait(motor_node_id, false);
            if (!disabled || !disabled->bit_value || *disabled->bit_value) {
                std::cerr << "ВНИМАНИЕ: выключение драйвера не подтверждено\n";
                return 1;
            }
            // Подтверждённый разработчиком баг прошивки: раннее повторное
            // is_on=1 после выключения может не сработать. Пауза нужна даже
            // после неудачной/прерванной пробы, если is_on=0 подтверждён.
            std::cout << "Пауза 10 секунд после is_on=0 из-за бага прошивки..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::cout << "Пауза завершена; пример можно запускать снова\n";
        } catch (const std::exception& e) {
            std::cerr << "ВНИМАНИЕ: ошибка при выключении: " << e.what() << '\n';
            return 1;
        }
    }
    const double mean = samples ? velocity_sum / samples : 0.0;
    const double variance = samples ? velocity_square_sum / samples - mean * mean : 0.0;
    std::cout << "Servo-проба " << (completed ? "завершена" : "прервана")
              << "; отсчётов " << samples << "; средняя скорость " << mean
              << " рад/с; СКО " << std::sqrt(std::max(0.0, variance))
              << "; минимум " << min_velocity << "; максимум " << max_velocity
              << "; изменение положения " << state.position_rad - start_position << " рад\n";
    std::cout << (command_attempted ? "Нулевая уставка отправлена" : "Servo-уставка не отправлялась")
              << "; is_on=0 подтверждено\n";
    return completed ? 0 : 1;
}
