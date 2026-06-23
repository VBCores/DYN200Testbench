#include <voltbro_testbench_client/all.hpp>

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace voltbro::testbench;

namespace {

// Пример без CLI: все параметры задаются здесь.
// Он включает мотор, одну секунду задает вращение в одну сторону,
// затем одну секунду задает вращение в другую сторону, собирая телеметрию в массивы.
constexpr const char* kIface = "vcan1.0";
constexpr uint8_t kLocalNodeId = 101;
constexpr uint8_t kMotorNodeId = 11;

constexpr float kVelocityForwardRadS = 5.0F;
constexpr float kVelocityReverseRadS = -5.0F;
constexpr int kStageMs = 1000;

// Коэффициенты FOC-регулятора для velocity-only команды.
constexpr float kTorqueNm = 0.0F;
constexpr float kAngleRad = 0.0F;
constexpr float kAngleKp = 0.0F;
constexpr float kVelocityKp = 6.0F;
constexpr float kCurrentKp = 3.0F;
constexpr float kCurrentKi = 1300.0F;

struct SampleArrays {
    std::vector<double> t_s;
    std::vector<float> position_rad;
    std::vector<float> velocity_rad_s;
    std::vector<float> torque_Nm;
};

struct LatestSample {
    bool valid{};
    double t_s{};
    float position_rad{};
    float velocity_rad_s{};
    float torque_Nm{};
};

float optionalOrNan(const std::optional<float>& value) {
    return value.value_or(std::numeric_limits<float>::quiet_NaN());
}

VbdriveFocCommand makeVelocityCommand(float velocity_rad_s) {
    VbdriveFocCommand cmd;
    cmd.torque_Nm = kTorqueNm;
    cmd.angle_rad = kAngleRad;
    cmd.velocity_rad_s = velocity_rad_s;
    cmd.angle_kp = kAngleKp;
    cmd.velocity_kp = kVelocityKp;
    cmd.current_kp = kCurrentKp;
    cmd.current_ki = kCurrentKi;
    return cmd;
}

void printStageSummary(const std::string& name, const SampleArrays& samples) {
    std::cout << name << ": collected " << samples.t_s.size() << " motor samples";
    if (!samples.t_s.empty()) {
        const auto last = samples.t_s.size() - 1U;
        std::cout << ", last pos=" << samples.position_rad[last]
                  << " rad, vel=" << samples.velocity_rad_s[last]
                  << " rad/s, torque=" << samples.torque_Nm[last] << " Nm";
    }
    std::cout << "\n";
}

}  // namespace

int main() {
    try {
        std::cout << "Открываем " << kIface << " и создаем клиентов\n";
        auto bus = makeCyphalInterface(kIface, kLocalNodeId);
        VbdriveClient motor(bus);

        SampleArrays forward;
        SampleArrays reverse;
        SampleArrays* active_samples = nullptr;
        LatestSample latest;
        auto stage_start = std::chrono::steady_clock::now();

        // Callback вызывается из bus->loop() на каждую принятую телеметрию subject 3811.
        // Здесь мы складываем данные в массивы текущей стадии. В реальном приложении
        // эти массивы можно потом отдать в анализатор, фильтр, plotter или сохранить в файл.
        motor.onState([&](const VbdriveState& state) {
            if (active_samples == nullptr) {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            const double t_s = std::chrono::duration<double>(now - stage_start).count();

            const float position = optionalOrNan(state.position_rad);
            const float velocity = optionalOrNan(state.velocity_rad_s);
            const float torque = optionalOrNan(state.torque_Nm);

            active_samples->t_s.push_back(t_s);
            active_samples->position_rad.push_back(position);
            active_samples->velocity_rad_s.push_back(velocity);
            active_samples->torque_Nm.push_back(torque);

            latest = LatestSample{true, t_s, position, velocity, torque};
        });

        std::cout << "Включаем мотор через register state.is_on\n";
        auto enable_result = motor.setMotorEnabledAndWait(
            kMotorNodeId,
            true,
            std::chrono::milliseconds(1000));
        if (!enable_result) {
            std::cerr << "Нет ответа state.is_on от мотора node-id "
                      << unsigned(kMotorNodeId) << "\n";
            return 1;
        }

        auto runStage = [&](const std::string& name, float velocity, SampleArrays& storage) {
            active_samples = &storage;
            latest = {};
            stage_start = std::chrono::steady_clock::now();
            auto next_print = stage_start;
            const auto deadline = stage_start + std::chrono::milliseconds(kStageMs);

            std::cout << "\n" << name << ": отправляем velocity=" << velocity << " rad/s\n";
            motor.sendFocCommand(kMotorNodeId, makeVelocityCommand(velocity));

            while (std::chrono::steady_clock::now() < deadline) {
                bus->loop();

                const auto now = std::chrono::steady_clock::now();
                if (now >= next_print) {
                    // Печатаем не каждую точку, а последнюю принятую точку примерно 10 раз в секунду.
                    // Так мы не мешаем приему телеметрии, которая может идти около 1 кГц.
                    if (latest.valid) {
                        std::cout << name
                                  << " t=" << latest.t_s
                                  << " pos=" << latest.position_rad
                                  << " vel=" << latest.velocity_rad_s
                                  << " torque=" << latest.torque_Nm << "\n";
                    } else {
                        std::cout << name << " waiting for telemetry...\n";
                    }
                    next_print = now + std::chrono::milliseconds(100);
                }
            }
            active_samples = nullptr;
            printStageSummary(name, storage);
        };

        runStage("forward", kVelocityForwardRadS, forward);
        runStage("reverse", kVelocityReverseRadS, reverse);

        std::cout << "\nОстанавливаем и выключаем мотор\n";
        motor.stopMotor(kMotorNodeId);
        motor.disableMotor(kMotorNodeId);
        flushCyphalTx(bus);

        std::cout << "Итого: forward=" << forward.t_s.size()
                  << " samples, reverse=" << reverse.t_s.size() << " samples\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
}
