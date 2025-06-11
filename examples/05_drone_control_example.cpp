// 05_drone_control_example.cpp - 드론 비행 제어 시스템
// 이 예제는 실시간 다축 제어 시스템에서의 Actor Model 사용을 보여줍니다

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>
#include <cmath>

using namespace mini_so;

// ============================================================================
// 드론 제어 메시지 정의
// ============================================================================
struct FlightData {
    float altitude;          // 고도 (m)
    float pitch;            // 피치 각도 (도)
    float roll;             // 롤 각도 (도) 
    float yaw;              // 요 각도 (도)
    float velocity_x;       // X축 속도 (m/s)
    float velocity_y;       // Y축 속도 (m/s)
    float velocity_z;       // Z축 속도 (m/s)
    uint32_t timestamp;
};

struct IMUData {
    float accel_x, accel_y, accel_z;     // 가속도 (m/s²)
    float gyro_x, gyro_y, gyro_z;        // 각속도 (deg/s)
    float mag_x, mag_y, mag_z;           // 자기장 (μT)
    float temperature;                   // 온도 (°C)
    bool calibrated;                     // 캘리브레이션 상태
};

struct GPSData {
    double latitude;         // 위도
    double longitude;        // 경도  
    float altitude;          // GPS 고도 (m)
    float speed;            // 속도 (m/s)
    float heading;          // 방향 (도)
    uint8_t satellites;     // 위성 수
    bool fix_valid;         // GPS 고정 유효성
};

struct ControlCommand {
    enum Type : uint8_t {
        TAKEOFF,
        LAND,
        HOVER,
        MOVE_TO_POSITION,
        ROTATE,
        EMERGENCY_LAND,
        RETURN_TO_HOME
    } command;
    
    float target_x, target_y, target_z;  // 목표 위치
    float target_yaw;                     // 목표 방향
    float speed;                          // 이동 속도
    bool immediate;                       // 즉시 실행 여부
};

struct MotorCommand {
    uint8_t motor_id;        // 모터 ID (0-3)
    uint16_t pwm_value;      // PWM 값 (1000-2000)
    bool emergency_stop;     // 비상 정지
};

struct DroneStatus {
    enum State : uint8_t {
        GROUNDED,
        TAKING_OFF,
        HOVERING,
        FLYING,
        LANDING,
        EMERGENCY
    } current_state;
    
    float battery_voltage;   // 배터리 전압 (V)
    uint8_t battery_percent; // 배터리 잔량 (%)
    bool motors_armed;       // 모터 활성화 상태
    bool gps_lock;          // GPS 고정 상태
    uint32_t flight_time;   // 비행 시간 (초)
};

struct FlightAlert {
    enum Priority : uint8_t { INFO, WARNING, CRITICAL, EMERGENCY } priority;
    enum Type : uint8_t {
        LOW_BATTERY,
        GPS_LOST,
        IMU_ERROR,
        MOTOR_FAILURE,
        ALTITUDE_LIMIT,
        WIND_STRONG,
        OBSTACLE_DETECTED
    } alert_type;
    
    const char* message;
    float value;
    bool requires_action;
};

// ============================================================================
// IMU 센서 Agent
// ============================================================================
class IMUAgent : public Agent {
private:
    uint32_t reading_count_ = 0;
    bool calibrated_ = false;
    float gyro_bias_x_ = 0.0f, gyro_bias_y_ = 0.0f, gyro_bias_z_ = 0.0f;
    IMUData current_data_{};
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(ControlCommand, handle_control_command);
        return false;
    }
    
    void read_imu_data() {
        reading_count_++;
        
        // IMU 데이터 시뮬레이션
        float time_factor = reading_count_ * 0.02f;
        
        // 가속도계 시뮬레이션 (중력 + 노이즈 + 움직임)
        current_data_.accel_x = 0.2f * sin(time_factor) + (rand() % 20 - 10) * 0.01f;
        current_data_.accel_y = 0.3f * cos(time_factor * 1.2f) + (rand() % 20 - 10) * 0.01f;
        current_data_.accel_z = 9.81f + 0.5f * sin(time_factor * 0.8f) + (rand() % 30 - 15) * 0.01f;
        
        // 자이로스코프 시뮬레이션 (각속도)
        current_data_.gyro_x = (0.5f * cos(time_factor * 0.7f) - gyro_bias_x_) + (rand() % 10 - 5) * 0.1f;
        current_data_.gyro_y = (0.8f * sin(time_factor * 0.9f) - gyro_bias_y_) + (rand() % 10 - 5) * 0.1f;
        current_data_.gyro_z = (0.3f * sin(time_factor * 1.1f) - gyro_bias_z_) + (rand() % 10 - 5) * 0.1f;
        
        // 자기장 센서 시뮬레이션 (지구 자기장)
        current_data_.mag_x = 22.5f + 2.0f * cos(time_factor * 0.1f);
        current_data_.mag_y = 5.8f + 1.5f * sin(time_factor * 0.15f);
        current_data_.mag_z = -42.3f + 3.0f * cos(time_factor * 0.08f);
        
        // 온도 시뮬레이션
        current_data_.temperature = 25.0f + 5.0f * sin(time_factor * 0.01f) + (rand() % 10 - 5) * 0.1f;
        
        // 캘리브레이션 체크
        if (!calibrated_ && reading_count_ > 50) {
            perform_calibration();
        }
        
        current_data_.calibrated = calibrated_;
        
        // 이상 상황 감지
        if (abs(current_data_.gyro_x) > 100.0f || abs(current_data_.gyro_y) > 100.0f) {
            FlightAlert alert{
                FlightAlert::CRITICAL,
                FlightAlert::IMU_ERROR,
                "과도한 각속도 감지",
                std::max({abs(current_data_.gyro_x), abs(current_data_.gyro_y), abs(current_data_.gyro_z)}),
                true
            };
            MINI_SO_BROADCAST(alert);
            printf("🚨 IMU: 위험한 각속도 감지! (%.1f, %.1f, %.1f) deg/s\n",
                   current_data_.gyro_x, current_data_.gyro_y, current_data_.gyro_z);
        }
        
        // 정상 데이터 주기적 출력
        if (reading_count_ % 25 == 0) {
            printf("📊 IMU: Accel(%.1f,%.1f,%.1f) Gyro(%.1f,%.1f,%.1f) Temp:%.1f°C %s\n",
                   current_data_.accel_x, current_data_.accel_y, current_data_.accel_z,
                   current_data_.gyro_x, current_data_.gyro_y, current_data_.gyro_z,
                   current_data_.temperature,
                   calibrated_ ? "CAL" : "UNCAL");
        }
        
        MINI_SO_BROADCAST(current_data_);
        
        MINI_SO_RECORD_PERFORMANCE(120, 1);  // 120μs 처리 시간
        MINI_SO_HEARTBEAT();
    }
    
private:
    void perform_calibration() {
        // 자이로 바이어스 계산 (평균값)
        gyro_bias_x_ = 0.1f;  // 시뮬레이션된 바이어스
        gyro_bias_y_ = -0.2f;
        gyro_bias_z_ = 0.05f;
        
        calibrated_ = true;
        printf("✅ IMU: 캘리브레이션 완료 (바이어스: %.2f, %.2f, %.2f)\n",
               gyro_bias_x_, gyro_bias_y_, gyro_bias_z_);
    }
    
    void handle_control_command(const ControlCommand& cmd) {
        if (cmd.command == ControlCommand::EMERGENCY_LAND) {
            printf("🛑 IMU: 비상착륙 모드 - 고주파 모니터링\n");
        }
    }
};

// ============================================================================
// GPS Agent
// ============================================================================
class GPSAgent : public Agent {
private:
    uint32_t reading_count_ = 0;
    GPSData current_position_{};
    double home_lat_ = 37.5665, home_lon_ = 126.9780;  // 서울 기준
    bool first_fix_ = false;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        return false;  // GPS는 독립적으로 동작
    }
    
    void update_gps_position() {
        reading_count_++;
        
        // GPS 시뮬레이션 (서울 주변에서 움직임)
        float time_factor = reading_count_ * 0.001f;
        
        // 위성 수 시뮬레이션 (4-12개)
        current_position_.satellites = 8 + (reading_count_ % 5);
        current_position_.fix_valid = (current_position_.satellites >= 4);
        
        if (current_position_.fix_valid) {
            // 위치 시뮬레이션 (작은 영역에서 움직임)
            current_position_.latitude = home_lat_ + 0.001f * sin(time_factor);
            current_position_.longitude = home_lon_ + 0.001f * cos(time_factor * 1.2f);
            current_position_.altitude = 50.0f + 20.0f * sin(time_factor * 0.5f);
            current_position_.speed = 2.0f + 3.0f * cos(time_factor * 0.8f);
            current_position_.heading = 180.0f + 90.0f * sin(time_factor * 0.3f);
            
            if (!first_fix_) {
                first_fix_ = true;
                printf("🛰️ GPS: 첫 고정 성공! (위성: %u개)\n", current_position_.satellites);
                printf("📍 홈 위치: %.6f, %.6f\n", home_lat_, home_lon_);
            }
        } else {
            printf("📡 GPS: 신호 손실 (위성: %u개)\n", current_position_.satellites);
            
            FlightAlert alert{
                FlightAlert::WARNING,
                FlightAlert::GPS_LOST,
                "GPS 신호 약함",
                static_cast<float>(current_position_.satellites),
                false
            };
            MINI_SO_BROADCAST(alert);
        }
        
        // GPS 상태 주기적 출력
        if (reading_count_ % 30 == 0 && current_position_.fix_valid) {
            printf("🛰️ GPS: %.6f,%.6f Alt:%.1fm Speed:%.1fm/s Heading:%.1f° (위성:%u)\n",
                   current_position_.latitude, current_position_.longitude,
                   current_position_.altitude, current_position_.speed,
                   current_position_.heading, current_position_.satellites);
        }
        
        MINI_SO_BROADCAST(current_position_);
        
        MINI_SO_RECORD_PERFORMANCE(100, 1);  // 100μs 처리 시간
        if (reading_count_ % 5 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
    
    GPSData get_home_position() const {
        GPSData home = current_position_;
        home.latitude = home_lat_;
        home.longitude = home_lon_;
        home.altitude = 0.0f;
        return home;
    }
};

// ============================================================================
// 비행 제어 Agent
// ============================================================================
class FlightControllerAgent : public Agent {
private:
    DroneStatus status_;
    IMUData last_imu_;
    GPSData last_gps_;
    FlightData current_flight_;
    uint32_t control_cycle_ = 0;
    
    // PID 제어기 상태
    float altitude_target_ = 0.0f;
    float altitude_error_integral_ = 0.0f;
    float last_altitude_error_ = 0.0f;
    
public:
    FlightControllerAgent() {
        status_.current_state = DroneStatus::GROUNDED;
        status_.battery_voltage = 12.6f;
        status_.battery_percent = 85;
        status_.motors_armed = false;
        status_.gps_lock = false;
        status_.flight_time = 0;
    }
    
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(IMUData, update_imu_data);
        HANDLE_MESSAGE_VOID(GPSData, update_gps_data);
        HANDLE_MESSAGE_VOID(ControlCommand, execute_control_command);
        HANDLE_MESSAGE_VOID(FlightAlert, handle_flight_alert);
        return false;
    }
    
    void run_control_loop() {
        control_cycle_++;
        
        // 배터리 시뮬레이션 (천천히 감소)
        if (status_.motors_armed && control_cycle_ % 100 == 0) {
            status_.battery_percent = std::max(0, static_cast<int>(status_.battery_percent) - 1);
            status_.battery_voltage = 10.5f + (status_.battery_percent * 2.1f / 100.0f);
            
            if (status_.battery_percent <= 20) {
                FlightAlert alert{
                    status_.battery_percent <= 10 ? FlightAlert::CRITICAL : FlightAlert::WARNING,
                    FlightAlert::LOW_BATTERY,
                    "배터리 부족",
                    static_cast<float>(status_.battery_percent),
                    status_.battery_percent <= 10
                };
                MINI_SO_BROADCAST(alert);
                
                if (status_.battery_percent <= 10) {
                    printf("🔋 자동 착륙 시작 (배터리: %u%%)\n", status_.battery_percent);
                    execute_emergency_landing();
                }
            }
        }
        
        // 비행 시간 업데이트
        if (status_.motors_armed) {
            status_.flight_time = control_cycle_ / 10;  // 0.1초 단위
        }
        
        // 상태별 제어 로직
        switch (status_.current_state) {
            case DroneStatus::GROUNDED:
                handle_grounded_state();
                break;
            case DroneStatus::TAKING_OFF:
                handle_takeoff_state();
                break;
            case DroneStatus::HOVERING:
                handle_hovering_state();
                break;
            case DroneStatus::FLYING:
                handle_flying_state();
                break;
            case DroneStatus::LANDING:
                handle_landing_state();
                break;
            case DroneStatus::EMERGENCY:
                handle_emergency_state();
                break;
        }
        
        // 상태 정보 주기적 브로드캐스트
        if (control_cycle_ % 50 == 0) {
            MINI_SO_BROADCAST(status_);
            printf("🚁 상태: %s, 고도: %.1fm, 배터리: %u%%, 비행시간: %us\n",
                   get_state_string(), current_flight_.altitude,
                   status_.battery_percent, status_.flight_time);
        }
        
        MINI_SO_RECORD_PERFORMANCE(250, 1);  // 250μs 제어 루프
        MINI_SO_HEARTBEAT();
    }
    
private:
    void update_imu_data(const IMUData& imu) {
        last_imu_ = imu;
        
        // IMU 데이터로부터 자세 계산 (간단한 시뮬레이션)
        current_flight_.pitch = last_imu_.accel_y * 5.0f;  // 간단한 근사
        current_flight_.roll = -last_imu_.accel_x * 5.0f;
        
        // 고도 추정 (가속도계 Z축 기반 간단한 추정)
        static float velocity_z = 0.0f;
        float accel_z_corrected = last_imu_.accel_z - 9.81f;
        velocity_z += accel_z_corrected * 0.01f;  // 10ms 적분
        current_flight_.altitude += velocity_z * 0.01f;
        
        // 고도 제한
        current_flight_.altitude = std::max(0.0f, current_flight_.altitude);
    }
    
    void update_gps_data(const GPSData& gps) {
        last_gps_ = gps;
        status_.gps_lock = gps.fix_valid;
        
        if (gps.fix_valid) {
            current_flight_.velocity_x = gps.speed * cos(gps.heading * M_PI / 180.0f);
            current_flight_.velocity_y = gps.speed * sin(gps.heading * M_PI / 180.0f);
        }
    }
    
    void execute_control_command(const ControlCommand& cmd) {
        printf("🎮 제어명령: %s\n", get_command_string(cmd.command));
        
        switch (cmd.command) {
            case ControlCommand::TAKEOFF:
                if (status_.current_state == DroneStatus::GROUNDED && status_.gps_lock) {
                    status_.current_state = DroneStatus::TAKING_OFF;
                    altitude_target_ = cmd.target_z > 0 ? cmd.target_z : 10.0f;
                    status_.motors_armed = true;
                    printf("🚁 이륙 시작 (목표 고도: %.1fm)\n", altitude_target_);
                } else {
                    printf("❌ 이륙 불가 (GPS: %s, 상태: %s)\n",
                           status_.gps_lock ? "OK" : "NO LOCK", get_state_string());
                }
                break;
                
            case ControlCommand::LAND:
                status_.current_state = DroneStatus::LANDING;
                altitude_target_ = 0.0f;
                printf("🛬 착륙 시작\n");
                break;
                
            case ControlCommand::HOVER:
                if (status_.current_state == DroneStatus::FLYING) {
                    status_.current_state = DroneStatus::HOVERING;
                    printf("🚁 호버링 모드\n");
                }
                break;
                
            case ControlCommand::EMERGENCY_LAND:
                execute_emergency_landing();
                break;
                
            case ControlCommand::RETURN_TO_HOME:
                // RTH 로직 구현
                printf("🏠 홈으로 복귀 시작\n");
                status_.current_state = DroneStatus::FLYING;
                break;
        }
    }
    
    void handle_flight_alert(const FlightAlert& alert) {
        if (alert.priority >= FlightAlert::CRITICAL) {
            printf("🚨 중요 경고: %s\n", alert.message);
            
            if (alert.requires_action) {
                execute_emergency_landing();
            }
        }
    }
    
    void handle_grounded_state() {
        status_.motors_armed = false;
        current_flight_.altitude = 0.0f;
    }
    
    void handle_takeoff_state() {
        // 고도 제어 (간단한 PID)
        float altitude_error = altitude_target_ - current_flight_.altitude;
        altitude_error_integral_ += altitude_error * 0.01f;
        float altitude_derivative = (altitude_error - last_altitude_error_) / 0.01f;
        
        float thrust = 0.5f + 0.1f * altitude_error + 0.01f * altitude_error_integral_ + 0.05f * altitude_derivative;
        thrust = std::max(0.0f, std::min(1.0f, thrust));
        
        // 모터 명령 생성
        for (int i = 0; i < 4; ++i) {
            MotorCommand motor_cmd{
                static_cast<uint8_t>(i),
                static_cast<uint16_t>(1000 + thrust * 1000),
                false
            };
            MINI_SO_BROADCAST(motor_cmd);
        }
        
        // 이륙 완료 확인
        if (abs(altitude_error) < 0.5f) {
            status_.current_state = DroneStatus::HOVERING;
            printf("✅ 이륙 완료 (고도: %.1fm)\n", current_flight_.altitude);
        }
        
        last_altitude_error_ = altitude_error;
    }
    
    void handle_hovering_state() {
        // 호버링 제어 (위치 유지)
        static uint32_t hover_cycles = 0;
        hover_cycles++;
        
        if (hover_cycles % 100 == 0) {
            printf("🚁 호버링 중 (고도: %.1fm, 자세: P:%.1f° R:%.1f°)\n",
                   current_flight_.altitude, current_flight_.pitch, current_flight_.roll);
        }
    }
    
    void handle_flying_state() {
        // 일반 비행 제어
        printf("✈️ 비행 중\n");
    }
    
    void handle_landing_state() {
        // 착륙 제어
        current_flight_.altitude -= 1.0f;  // 1m/s 하강
        
        if (current_flight_.altitude <= 0.1f) {
            status_.current_state = DroneStatus::GROUNDED;
            status_.motors_armed = false;
            printf("🛬 착륙 완료\n");
        }
    }
    
    void handle_emergency_state() {
        // 비상 착륙
        current_flight_.altitude -= 2.0f;  // 빠른 하강
        
        if (current_flight_.altitude <= 0.1f) {
            status_.current_state = DroneStatus::GROUNDED;
            status_.motors_armed = false;
            printf("🚨 비상착륙 완료\n");
        }
    }
    
    void execute_emergency_landing() {
        status_.current_state = DroneStatus::EMERGENCY;
        printf("🚨 비상착륙 시작!\n");
        
        // 모든 모터 즉시 정지
        for (int i = 0; i < 4; ++i) {
            MotorCommand emergency_cmd{
                static_cast<uint8_t>(i),
                1000,  // 최소 PWM
                true   // 비상 정지
            };
            MINI_SO_BROADCAST(emergency_cmd);
        }
    }
    
    const char* get_state_string() const {
        switch (status_.current_state) {
            case DroneStatus::GROUNDED: return "지상";
            case DroneStatus::TAKING_OFF: return "이륙중";
            case DroneStatus::HOVERING: return "호버링";
            case DroneStatus::FLYING: return "비행중";
            case DroneStatus::LANDING: return "착륙중";
            case DroneStatus::EMERGENCY: return "비상";
            default: return "알수없음";
        }
    }
    
    const char* get_command_string(ControlCommand::Type cmd) const {
        switch (cmd) {
            case ControlCommand::TAKEOFF: return "이륙";
            case ControlCommand::LAND: return "착륙";
            case ControlCommand::HOVER: return "호버링";
            case ControlCommand::MOVE_TO_POSITION: return "위치이동";
            case ControlCommand::ROTATE: return "회전";
            case ControlCommand::EMERGENCY_LAND: return "비상착륙";
            case ControlCommand::RETURN_TO_HOME: return "홈복귀";
            default: return "알수없음";
        }
    }
};

// ============================================================================
// 메인 함수: 드론 제어 시뮬레이션
// ============================================================================
int main() {
    printf("🚁 드론 비행 제어 시스템 🚁\n");
    printf("========================\n\n");
    
    // 시스템 초기화
    MINI_SO_INIT();
    if (!sys.initialize()) {
        printf("❌ 드론 시스템 초기화 실패!\n");
        return 1;
    }
    printf("✅ 드론 비행 제어 시스템 초기화 완료\n\n");
    
    // 드론 구성 요소 Agent들 생성
    IMUAgent imu;
    GPSAgent gps;
    FlightControllerAgent flight_controller;
    
    // Agent 등록
    MINI_SO_REGISTER(imu);
    MINI_SO_REGISTER(gps);
    MINI_SO_REGISTER(flight_controller);
    
    printf("🔧 드론 시스템 구성 요소 활성화:\n");
    printf("   - IMU: 관성 측정 장치\n");
    printf("   - GPS: 위치 항법 시스템\n");
    printf("   - Flight Controller: 비행 제어 컴퓨터\n\n");
    
    printf("🚀 드론 시스템 시작!\n");
    printf("==========================================\n");
    
    // 비행 시뮬레이션 실행
    for (int cycle = 0; cycle < 60; ++cycle) {
        printf("\n--- 비행 사이클 %d (%.1f초) ---\n", cycle + 1, cycle * 0.1f);
        
        // 센서 데이터 수집
        imu.read_imu_data();
        gps.update_gps_position();
        
        // 비행 제어 실행
        flight_controller.run_control_loop();
        
        // 메시지 처리
        MINI_SO_RUN();
        
        // 시뮬레이션 이벤트 발생
        if (cycle == 10) {
            printf("\n🚁 이륙 명령 전송\n");
            ControlCommand takeoff{ControlCommand::TAKEOFF, 0, 0, 15.0f, 0, 2.0f, false};
            env.send_message(3, 3, takeoff);
        }
        
        if (cycle == 35) {
            printf("\n🛬 착륙 명령 전송\n");
            ControlCommand land{ControlCommand::LAND, 0, 0, 0, 0, 1.0f, false};
            env.send_message(3, 3, land);
        }
        
        // 주기적 시스템 상태 확인
        if (cycle % 20 == 19) {
            printf("\n📊 시스템 상태 점검 (%.1f초):\n", cycle * 0.1f);
            printf("   - 비행 제어 시스템: %s\n", sys.is_healthy() ? "정상" : "점검 필요");
            printf("   - 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
        }
    }
    
    printf("\n==========================================\n");
    printf("📊 비행 세션 완료\n");
    printf("==========================================\n");
    
    printf("\n📈 드론 시스템 성능:\n");
    printf("   - 총 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("   - 최대 처리 시간: %u μs\n", sys.performance().max_processing_time_us());
    printf("   - 시스템 에러: %zu\n", sys.error().error_count());
    printf("   - 비행 안전성: %s\n", sys.is_healthy() ? "우수" : "점검 필요");
    
    printf("\n🎯 드론 제어 시뮬레이션 완료!\n");
    printf("💡 이 예제에서 배운 내용:\n");
    printf("   ✅ 실시간 센서 융합 (IMU + GPS)\n");
    printf("   ✅ 다축 비행 제어 시스템\n");
    printf("   ✅ PID 제어기 구현\n");
    printf("   ✅ 비상 안전 시스템\n");
    printf("   ✅ 배터리 관리 및 자동 착륙\n");
    printf("   ✅ 실시간 상태 머신 제어\n");
    
    return 0;
}