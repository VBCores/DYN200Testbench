#include <voltbro_testbench_client/all.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

using namespace voltbro::testbench;

namespace {

// Это демонстрационный пример без обработки CLI.
// Чтобы поменять интерфейс, node-id, длительность или уставку мотора,
// редактируйте эти константы и пересобирайте пример.
constexpr const char* kIface = "vcan1.0";
constexpr uint8_t kLocalNodeId = 101;
constexpr uint8_t kMotorNodeId = 11;
constexpr int kRunSeconds = 5;

constexpr const char* kMotorCsvPath = "simple_motor.csv";
constexpr const char* kDyn200CsvPath = "simple_dyn200.csv";

// ВНИМАНИЕ: команда ниже реально включает мотор и отправляет FOC-уставку.
// Запускайте пример только когда стенд физически безопасен.
constexpr float kTorqueNm = 0.0F;
constexpr float kAngleRad = 0.0F;
constexpr float kVelocityRadS = 10.0F;
constexpr float kAngleKp = 0.0F;
constexpr float kVelocityKp = 6.0F;
constexpr float kCurrentKp = 3.0F;
constexpr float kCurrentKi = 1300.0F;

template <typename T>
std::string valueOrEmpty(const std::optional<T>& value) {
    return value ? std::to_string(*value) : std::string{};
}

void writeMotorHeader(std::ofstream& out) {
    out << "host_time_ns,iface,source_node_id,subject_id,transfer_id,timestamp_us,"
           "position_rad,velocity_rad_s,torque_Nm,current_A,voltage_V,"
           "temperature_C,mcu_temperature_C,stator_temperature_C,has_fault\n";
}

void writeDyn200Header(std::ofstream& out) {
    out << "host_time_ns,iface,source,source_node_id,subject_id,transfer_id,"
           "sample_counter,timestamp_us,angular_velocity_rad_s,torque_Nm,power_W,"
           "raw_speed,raw_torque,raw_power,status_flags,actual_acq_rate_hz,"
           "publication_rate_hz,crc_error_count,timeout_count,uart_error_count,notes\n";
}

}  // namespace

int main() {
    try {
        std::ofstream motor_csv(kMotorCsvPath);
        std::ofstream dyn200_csv(kDyn200CsvPath);
        if (!motor_csv || !dyn200_csv) {
            std::cerr << "Не удалось открыть CSV-файлы для записи\n";
            return 1;
        }
        writeMotorHeader(motor_csv);
        writeDyn200Header(dyn200_csv);

        // Один объект CyphalInterface открывает SocketCAN-интерфейс.
        // В текущей топологии и мотор, и DYN-200 находятся на одной шине vcan1.0.
        auto bus = makeCyphalInterface(kIface, kLocalNodeId);

        // Клиенты подписываются на свои сообщения при создании:
        // VbdriveClient читает subject 3811 и умеет отправлять команды мотору,
        // Dyn200Client читает subject 5100/5101 и умеет отправлять команды DYN-200.
        VbdriveClient motor(bus);
        Dyn200Client dyn200(bus);

        // Callback вызывается из bus->loop(), когда пришла новая телеметрия мотора.
        // Здесь мы сразу пишем строку в motor CSV. Лишнюю работу в callback лучше не делать,
        // чтобы не задерживать прием высокочастотной телеметрии.
        motor.onState([&](const VbdriveState& state) {
            motor_csv << steadyTimeNs(state.host_receive_time) << "," << kIface << ","
                      << unsigned(state.source_node_id) << "," << kVbdriveStateSimpleSubjectId << ","
                      << state.transfer_id << "," << valueOrEmpty(state.timestamp_us) << ","
                      << valueOrEmpty(state.position_rad) << "," << valueOrEmpty(state.velocity_rad_s) << ","
                      << valueOrEmpty(state.torque_Nm) << "," << valueOrEmpty(state.current_A) << ","
                      << valueOrEmpty(state.voltage_V) << "," << valueOrEmpty(state.temperature_C) << ","
                      << valueOrEmpty(state.mcu_temperature_C) << ","
                      << valueOrEmpty(state.stator_temperature_C) << ","
                      << (state.has_fault ? 1 : 0) << "\n";
        });

        // DYN-200 state: основная измерительная телеметрия.
        dyn200.onState([&](const Dyn200State& state) {
            dyn200_csv << steadyTimeNs(state.host_receive_time) << "," << kIface << ",dyn200_state,"
                       << unsigned(state.source_node_id) << "," << kDyn200StateSubjectId << ","
                       << unsigned(state.transfer_id) << "," << state.sample_counter << ","
                       << state.timestamp_us << "," << state.angular_velocity_rad_s << ","
                       << state.torque_Nm << "," << state.power_W << "," << state.raw_speed << ","
                       << state.raw_torque << "," << state.raw_power << ","
                       << unsigned(state.status_flags) << ",,,,,ok\n";
        });

        // DYN-200 status: счетчики ошибок, фактические частоты и runtime-настройки.
        dyn200.onStatus([&](const Dyn200Status& status) {
            dyn200_csv << steadyTimeNs(status.host_receive_time) << "," << kIface << ",dyn200_status,"
                       << unsigned(status.source_node_id) << "," << kDyn200StatusSubjectId << ","
                       << unsigned(status.transfer_id) << "," << status.sample_counter
                       << ",,,,,,,,,"
                       << status.actual_acquisition_rate_hz << ","
                       << status.cyphal_publication_rate_hz << ","
                       << status.crc_error_count << "," << status.timeout_count << ","
                       << status.uart_error_count << ",ok\n";
        });

        std::cout << "Открыт " << kIface << ", пишем " << kMotorCsvPath
                  << " и " << kDyn200CsvPath << "\n";

        // Регистром state.is_on включаем мотор. Это service request к узлу kMotorNodeId.
        // Если ответа нет, пример прекращается: отправлять FOC-команду в непонятное состояние
        // обычно хуже, чем явно показать проблему связи.
        auto enable_result = motor.setMotorEnabledAndWait(
            kMotorNodeId,
            true,
            std::chrono::milliseconds(1000));
        if (!enable_result) {
            std::cerr << "Нет ответа state.is_on от мотора node-id "
                      << unsigned(kMotorNodeId) << "\n";
            return 1;
        }

        // Отправляем одну FOC-команду. libcxxcanard сам сериализует DSDL-тип,
        // ставит tail byte, transfer-id и корректный CAN FD DLC.
        VbdriveFocCommand foc;
        foc.torque_Nm = kTorqueNm;
        foc.angle_rad = kAngleRad;
        foc.velocity_rad_s = kVelocityRadS;
        foc.angle_kp = kAngleKp;
        foc.velocity_kp = kVelocityKp;
        foc.current_kp = kCurrentKp;
        foc.current_ki = kCurrentKi;
        motor.sendFocCommand(kMotorNodeId, foc);

        std::cout << "FOC отправлен: velocity=" << kVelocityRadS
                  << " rad/s, velocity_kp=" << kVelocityKp << "\n";

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kRunSeconds);
        while (std::chrono::steady_clock::now() < deadline) {
            // loop() принимает кадры, вызывает callback-и подписок и отправляет накопленные TX-кадры.
            // В этом примере нет sleep: poll внутри провайдера сам управляет ожиданием.
            bus->loop();
        }

        // Аккуратно останавливаем и выключаем мотор после окна записи.
        motor.stopMotor(kMotorNodeId);
        motor.disableMotor(kMotorNodeId);
        flushCyphalTx(bus);

        motor_csv.flush();
        dyn200_csv.flush();

        std::cout << "Готово. Строк мотора: " << motor.statistics().state_messages
                  << ", DYN-200 state: " << dyn200.statistics().state_messages
                  << ", DYN-200 status: " << dyn200.statistics().status_messages << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
}
