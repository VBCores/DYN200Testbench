#include <voltbro_testbench_client/dyn200.hpp>

#include <chrono>
#include <fstream>
#include <iostream>

using namespace voltbro::testbench;

int main() {
    // Все настройки — здесь, аргументов командной строки нет.
    // Стенд на vcan1.0; локальный ID должен быть свободным и отличаться от 79.
    constexpr const char* can_interface = "vcan1.0";
    constexpr uint8_t local_node_id = 101;
    constexpr uint8_t dyn200_node_id = 79;

    // Файл создаётся в текущем рабочем каталоге процесса.
    std::ofstream csv("dyn200.csv");
    if (!csv) {
        std::cerr << "Не удалось открыть dyn200.csv\n";
        return 1;
    }
    csv << "host_time_ms,subject_id,value,unit\n";

    auto bus = makeCyphalInterface(can_interface, local_node_id);
    Dyn200Client dyn200(bus, dyn200_node_id);
    unsigned speed_count = 0;
    unsigned torque_count = 0;

    // Время — монотонные часы ПК, а не Unix-время. Оно подходит для интервалов.
    // Скорость (5100, rpm) и момент (5101, Nm) пишутся отдельными строками:
    // соседние строки нельзя считать одной синхронной пробой.
    const auto write = [&](uint16_t subject, const Dyn200Measurement& data, const char* unit) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            data.host_receive_time.time_since_epoch()).count();
        csv << ms << ',' << subject << ',' << data.value << ',' << unit << '\n';
    };
    dyn200.onSpeed([&](const Dyn200Measurement& data) {
        ++speed_count;
        write(kDyn200SpeedSubjectId, data, "rpm");
    });
    dyn200.onTorque([&](const Dyn200Measurement& data) {
        ++torque_count;
        write(kDyn200TorqueSubjectId, data, "Nm");
    });

    // Колбэки с записью CSV вызываются внутри loop(); здесь читаем 10 секунд.
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end) {
        bus->loop();
    }
    std::cout << "dyn200.csv: скорость " << speed_count << ", момент " << torque_count << '\n';
    return speed_count && torque_count ? 0 : 1;
}
