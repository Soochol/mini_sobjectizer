// 03_autonomous_vehicle_example.cpp - 자율주행 차량 센서 융합 시스템
// 이 예제는 실시간 안전 중요 시스템에서의 Actor Model 사용을 보여줍니다

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>
#include <cmath>

using namespace mini_so;

// ============================================================================
// 자율주행 메시지 정의
// ============================================================================
struct LidarData {
    float distance_front;     // 전방 거리 (m)
    float distance_left;      // 좌측 거리 (m)
    float distance_right;     // 우측 거리 (m)
    uint32_t point_count;     // 스캔된 포인트 수
    bool obstacle_detected;   // 장애물 감지 여부
};

struct CameraData {
    enum ObjectType : uint8_t { NONE, CAR, PEDESTRIAN, TRAFFIC_LIGHT, ROAD_SIGN } detected_object;
    float object_distance;    // 객체까지 거리 (m)
    float object_speed;       // 객체 상대 속도 (m/s)
    bool lane_departure;      // 차선 이탈 감지
    uint8_t confidence;       // 신뢰도 (0-100)
};

struct RadarData {
    float range;              // 탐지 거리 (m)
    float relative_velocity;  // 상대 속도 (m/s)
    float angle;              // 각도 (도)
    bool target_lock;         // 타겟 고정 여부
};

struct VehicleState {
    float speed;              // 현재 속도 (km/h)
    float steering_angle;     // 조향각 (도)
    float acceleration;       // 가속도 (m/s²)
    bool braking;            // 브레이킹 여부
    bool emergency_stop;     // 비상 정지 여부
};

struct ControlCommand {
    enum Type : uint8_t { ACCELERATE, BRAKE, STEER, EMERGENCY_STOP, LANE_CHANGE } command;
    float parameter;         // 속도, 각도 등
    uint8_t urgency;         // 긴급도 (0-255)
};

struct SafetyAlert {
    enum Level : uint8_t { INFO, WARNING, CRITICAL, EMERGENCY } level;
    const char* message;
    float time_to_collision;  // 충돌까지 시간 (초)
    bool requires_immediate_action;
};

// ============================================================================
// LiDAR 센서 Agent
// ============================================================================
class LidarAgent : public Agent {
private:
    uint32_t scan_count_ = 0;
    float last_front_distance_ = 50.0f;
    bool calibrated_ = true;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(ControlCommand, handle_control_command);
        return false;
    }
    
    void perform_scan() {
        if (!calibrated_) return;
        
        scan_count_++;
        
        // LiDAR 스캔 시뮬레이션 (360도 스캔)
        float front_distance = 50.0f + 20.0f * sin(scan_count_ * 0.05f);
        float left_distance = 30.0f + 15.0f * cos(scan_count_ * 0.08f);
        float right_distance = 35.0f + 10.0f * sin(scan_count_ * 0.06f);
        
        // 동적 장애물 시뮬레이션
        if (scan_count_ > 50 && scan_count_ < 80) {
            front_distance = std::max(5.0f, front_distance - (scan_count_ - 50) * 0.8f);
        }
        
        bool obstacle = front_distance < 15.0f || left_distance < 8.0f || right_distance < 8.0f;
        
        LidarData data{
            front_distance, left_distance, right_distance,
            static_cast<uint32_t>(360 + scan_count_ % 100),  // 포인트 수
            obstacle
        };
        
        if (obstacle) {
            printf("🚨 LiDAR: 장애물 감지! 전방: %.1fm, 좌: %.1fm, 우: %.1fm\n",
                   front_distance, left_distance, right_distance);
        } else if (scan_count_ % 10 == 0) {
            printf("📡 LiDAR: 스캔 #%u - 전방: %.1fm (포인트: %u개)\n",
                   scan_count_, front_distance, data.point_count);
        }
        
        MINI_SO_BROADCAST(data);
        last_front_distance_ = front_distance;
        
        // 성능 메트릭 및 heartbeat
        MINI_SO_RECORD_PERFORMANCE(100, 1);  // 100μs 처리 시간
        if (scan_count_ % 5 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
    
private:
    void handle_control_command(const ControlCommand& cmd) {
        if (cmd.command == ControlCommand::EMERGENCY_STOP) {
            printf("🛑 LiDAR: 비상정지 - 스캔 일시 중단\n");
            calibrated_ = false;
        }
    }
};

// ============================================================================
// 카메라 비전 Agent
// ============================================================================
class CameraAgent : public Agent {
private:
    uint32_t frame_count_ = 0;
    CameraData::ObjectType last_detected_ = CameraData::NONE;
    bool lane_tracking_active_ = true;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(VehicleState, update_tracking_parameters);
        return false;
    }
    
    void process_frame() {
        if (!lane_tracking_active_) return;
        
        frame_count_++;
        
        // 컴퓨터 비전 처리 시뮬레이션
        CameraData::ObjectType detected = CameraData::NONE;
        float object_distance = 100.0f;
        float object_speed = 0.0f;
        uint8_t confidence = 95;
        
        // 주기적으로 다양한 객체 감지 시뮬레이션
        if (frame_count_ > 30 && frame_count_ < 50) {
            detected = CameraData::CAR;
            object_distance = 25.0f - (frame_count_ - 30) * 0.5f;
            object_speed = -2.0f;  // 접근 중
            confidence = 92;
        } else if (frame_count_ > 70 && frame_count_ < 85) {
            detected = CameraData::PEDESTRIAN;
            object_distance = 15.0f;
            object_speed = 1.5f;   // 횡단 중
            confidence = 88;
        } else if (frame_count_ > 100 && frame_count_ < 110) {
            detected = CameraData::TRAFFIC_LIGHT;
            object_distance = 40.0f;
            object_speed = 0.0f;
            confidence = 96;
        }
        
        // 차선 이탈 감지 시뮬레이션
        bool lane_departure = (frame_count_ % 47 == 0);  // 가끔 차선 이탈
        
        CameraData data{
            detected, object_distance, object_speed, lane_departure, confidence
        };
        
        // 중요한 객체 감지 시 알림
        if (detected != CameraData::NONE) {
            const char* object_name = "";
            switch (detected) {
                case CameraData::CAR: object_name = "차량"; break;
                case CameraData::PEDESTRIAN: object_name = "보행자"; break;
                case CameraData::TRAFFIC_LIGHT: object_name = "신호등"; break;
                case CameraData::ROAD_SIGN: object_name = "표지판"; break;
                default: object_name = "미확인"; break;
            }
            
            printf("📷 Camera: %s 감지 - 거리: %.1fm, 신뢰도: %u%%\n",
                   object_name, object_distance, confidence);
                   
            if (detected == CameraData::PEDESTRIAN && object_distance < 20.0f) {
                printf("⚠️ Camera: 보행자 접근 경고!\n");
            }
        }
        
        if (lane_departure) {
            printf("🛣️ Camera: 차선 이탈 감지!\n");
        }
        
        MINI_SO_BROADCAST(data);
        last_detected_ = detected;
        
        // 프레임 처리 성능 메트릭
        MINI_SO_RECORD_PERFORMANCE(150, 1);  // 150μs 처리 시간
        if (frame_count_ % 8 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
    
private:
    void update_tracking_parameters(const VehicleState& state) {
        // 고속에서는 더 멀리 보도록 조정
        if (state.speed > 80.0f) {
            // 고속 주행 모드
        } else if (state.speed < 20.0f) {
            // 저속/주차 모드
        }
    }
};

// ============================================================================
// 레이더 센서 Agent
// ============================================================================
class RadarAgent : public Agent {
private:
    uint32_t sweep_count_ = 0;
    bool target_locked_ = false;
    float last_range_ = 0.0f;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        (void)msg;  // 경고 방지
        return false;  // 독립적으로 동작
    }
    
    void perform_sweep() {
        sweep_count_++;
        
        // 레이더 스윕 시뮬레이션
        float range = 80.0f + 30.0f * cos(sweep_count_ * 0.1f);
        float relative_velocity = -5.0f + 10.0f * sin(sweep_count_ * 0.15f);
        float angle = -45.0f + 90.0f * sin(sweep_count_ * 0.05f);
        
        // 타겟 락온 시뮬레이션
        bool target_lock = (range < 50.0f && abs(relative_velocity) > 2.0f);
        
        RadarData data{range, relative_velocity, angle, target_lock};
        
        if (target_lock && !target_locked_) {
            printf("🎯 Radar: 타겟 락온! 거리: %.1fm, 상대속도: %.1fm/s\n",
                   range, relative_velocity);
        } else if (!target_lock && target_locked_) {
            printf("📡 Radar: 타겟 로스트\n");
        }
        
        if (sweep_count_ % 15 == 0) {
            printf("📡 Radar: 스윕 #%u - 거리: %.1fm, 각도: %.1f°\n",
                   sweep_count_, range, angle);
        }
        
        MINI_SO_BROADCAST(data);
        target_locked_ = target_lock;
        last_range_ = range;
        
        MINI_SO_RECORD_PERFORMANCE(80, 1);  // 80μs 처리 시간
        if (sweep_count_ % 6 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
};

// ============================================================================
// 센서 융합 및 판단 Agent
// ============================================================================
class FusionAgent : public Agent {
private:
    LidarData last_lidar_;
    CameraData last_camera_;
    RadarData last_radar_;
    VehicleState current_state_{60.0f, 0.0f, 0.0f, false, false};  // 초기 상태
    uint32_t fusion_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(LidarData, update_lidar_data);
        HANDLE_MESSAGE_VOID(CameraData, update_camera_data);
        HANDLE_MESSAGE_VOID(RadarData, update_radar_data);
        return false;
    }
    
    void perform_fusion_analysis() {
        fusion_count_++;
        
        // 센서 융합 알고리즘
        bool emergency_detected = false;
        float threat_level = 0.0f;
        float time_to_collision = 999.0f;
        
        // LiDAR 기반 위험 분석
        if (last_lidar_.obstacle_detected) {
            threat_level += 0.4f;
            if (last_lidar_.distance_front < 10.0f) {
                emergency_detected = true;
                time_to_collision = last_lidar_.distance_front / (current_state_.speed / 3.6f);
            }
        }
        
        // Camera 기반 위험 분석
        if (last_camera_.detected_object == CameraData::PEDESTRIAN) {
            threat_level += 0.6f;
            if (last_camera_.object_distance < 15.0f) {
                emergency_detected = true;
                time_to_collision = std::min(time_to_collision, 
                    last_camera_.object_distance / (current_state_.speed / 3.6f));
            }
        }
        
        if (last_camera_.lane_departure) {
            threat_level += 0.3f;
        }
        
        // Radar 기반 충돌 예측
        if (last_radar_.target_lock && last_radar_.relative_velocity < -3.0f) {
            threat_level += 0.5f;
            time_to_collision = std::min(time_to_collision,
                last_radar_.range / abs(last_radar_.relative_velocity));
        }
        
        // 위험도별 대응 결정
        if (emergency_detected || threat_level > 0.8f) {
            SafetyAlert alert{
                SafetyAlert::EMERGENCY,
                "즉시 비상정지 필요",
                time_to_collision,
                true
            };
            MINI_SO_BROADCAST(alert);
            
            ControlCommand emergency_cmd{ControlCommand::EMERGENCY_STOP, 0.0f, 255};
            MINI_SO_BROADCAST(emergency_cmd);
            
            current_state_.emergency_stop = true;
            current_state_.speed = 0.0f;
            
            printf("🚨 FUSION: 비상상황 감지! 즉시 정지 (위험도: %.1f, TTC: %.1fs)\n",
                   threat_level, time_to_collision);
                   
        } else if (threat_level > 0.5f) {
            SafetyAlert alert{
                SafetyAlert::CRITICAL,
                "감속 및 회피 기동 권장",
                time_to_collision,
                false
            };
            MINI_SO_BROADCAST(alert);
            
            ControlCommand brake_cmd{ControlCommand::BRAKE, 0.3f, 180};
            MINI_SO_BROADCAST(brake_cmd);
            
            printf("⚠️ FUSION: 위험 상황 - 감속 권장 (위험도: %.1f)\n", threat_level);
            
        } else if (threat_level > 0.2f) {
            SafetyAlert alert{
                SafetyAlert::WARNING,
                "주의 운전 필요",
                time_to_collision,
                false
            };
            MINI_SO_BROADCAST(alert);
            
            if (fusion_count_ % 20 == 0) {
                printf("💡 FUSION: 주의 필요 (위험도: %.1f)\n", threat_level);
            }
        }
        
        // 정상 상태에서의 주기적 상태 업데이트
        if (fusion_count_ % 25 == 0 && threat_level < 0.2f) {
            printf("✅ FUSION: 정상 주행 중 (속도: %.1fkm/h, 위험도: %.1f)\n",
                   current_state_.speed, threat_level);
        }
        
        // 차량 상태 브로드캐스트
        MINI_SO_BROADCAST(current_state_);
        
        MINI_SO_RECORD_PERFORMANCE(200, 3);  // 200μs, 3개 센서 융합
        MINI_SO_HEARTBEAT();
    }
    
private:
    void update_lidar_data(const LidarData& data) {
        last_lidar_ = data;
    }
    
    void update_camera_data(const CameraData& data) {
        last_camera_ = data;
    }
    
    void update_radar_data(const RadarData& data) {
        last_radar_ = data;
    }
};

// ============================================================================
// 안전 감시 Agent
// ============================================================================
class SafetyMonitorAgent : public Agent {
private:
    uint32_t alert_count_ = 0;
    uint32_t emergency_count_ = 0;
    uint32_t collision_avoidance_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(SafetyAlert, handle_safety_alert);
        HANDLE_MESSAGE_VOID(ControlCommand, monitor_control_command);
        return false;
    }
    
    void print_safety_summary() {
        printf("\n🛡️ 안전 시스템 요약:\n");
        printf("   - 총 안전 경고: %u회\n", alert_count_);
        printf("   - 비상 대응: %u회\n", emergency_count_);
        printf("   - 충돌 회피: %u회\n", collision_avoidance_count_);
        
        if (emergency_count_ == 0) {
            printf("   ✅ 무사고 주행 완료\n");
        } else {
            printf("   ⚠️ 비상 상황 발생 - 시스템 점검 권장\n");
        }
    }
    
private:
    void handle_safety_alert(const SafetyAlert& alert) {
        alert_count_++;
        
        const char* level_str = "";
        switch (alert.level) {
            case SafetyAlert::INFO: level_str = "정보"; break;
            case SafetyAlert::WARNING: level_str = "주의"; break;
            case SafetyAlert::CRITICAL: level_str = "위험"; break;
            case SafetyAlert::EMERGENCY: 
                level_str = "비상"; 
                emergency_count_++;
                break;
        }
        
        if (alert.level >= SafetyAlert::CRITICAL) {
            printf("🚨 안전경고 [%s]: %s (TTC: %.1fs)\n",
                   level_str, alert.message, alert.time_to_collision);
        }
        
        if (alert.requires_immediate_action) {
            collision_avoidance_count_++;
        }
        
        MINI_SO_HEARTBEAT();
    }
    
    void monitor_control_command(const ControlCommand& cmd) {
        if (cmd.command == ControlCommand::EMERGENCY_STOP) {
            printf("🛑 안전감시: 비상정지 명령 확인 (긴급도: %u)\n", cmd.urgency);
        }
    }
};

// ============================================================================
// 메인 함수: 자율주행 시뮬레이션
// ============================================================================
int main() {
    printf("🚗 자율주행 차량 센서 융합 시스템 🚗\n");
    printf("==================================\n\n");
    
    // 시스템 초기화
    MINI_SO_INIT();
    if (!sys.initialize()) {
        printf("❌ 자율주행 시스템 초기화 실패!\n");
        return 1;
    }
    printf("✅ 자율주행 시스템 초기화 완료\n\n");
    
    // 센서 Agent들 생성
    LidarAgent lidar;
    CameraAgent camera;
    RadarAgent radar;
    FusionAgent fusion;
    SafetyMonitorAgent safety_monitor;
    
    // Agent 등록
    MINI_SO_REGISTER(lidar);
    MINI_SO_REGISTER(camera);
    MINI_SO_REGISTER(radar);
    MINI_SO_REGISTER(fusion);
    MINI_SO_REGISTER(safety_monitor);
    
    printf("🔧 센서 시스템 활성화 완료\n");
    printf("   - LiDAR: 360도 스캔\n");
    printf("   - Camera: 객체 인식 및 차선 감지\n");
    printf("   - Radar: 거리 및 속도 측정\n");
    printf("   - Fusion: 센서 융합 분석\n");
    printf("   - Safety: 안전 감시\n\n");
    
    printf("🚀 자율주행 시작!\n");
    printf("==========================================\n");
    
    // 자율주행 시뮬레이션 실행
    for (int cycle = 0; cycle < 40; ++cycle) {
        printf("\n--- 주행 사이클 %d (%.1f초) ---\n", cycle + 1, cycle * 0.1f);
        
        // 센서 데이터 수집 (100Hz)
        lidar.perform_scan();
        camera.process_frame();
        radar.perform_sweep();
        
        // 센서 융합 및 판단 (50Hz)
        if (cycle % 2 == 0) {
            fusion.perform_fusion_analysis();
        }
        
        // 모든 메시지 처리
        MINI_SO_RUN();
        
        // 주기적 상태 확인
        if (cycle % 15 == 14) {
            printf("\n📊 시스템 상태 점검 (%.1f초):\n", cycle * 0.1f);
            printf("   - 시스템 상태: %s\n", sys.is_healthy() ? "정상" : "점검 필요");
            printf("   - 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
        }
    }
    
    printf("\n==========================================\n");
    printf("📊 자율주행 세션 완료\n");
    printf("==========================================\n");
    
    safety_monitor.print_safety_summary();
    
    printf("\n📈 시스템 성능 분석:\n");
    printf("   - 총 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("   - 최대 처리 시간: %u μs\n", sys.performance().max_processing_time_us());
    printf("   - 시스템 에러: %zu\n", sys.error().error_count());
    printf("   - 시스템 건강도: %s\n", sys.is_healthy() ? "우수" : "점검 필요");
    
    printf("\n🎯 자율주행 시뮬레이션 완료!\n");
    printf("💡 이 예제에서 배운 내용:\n");
    printf("   ✅ 다중 센서 실시간 데이터 처리\n");
    printf("   ✅ 센서 융합 알고리즘\n");
    printf("   ✅ 실시간 안전 판단 시스템\n");
    printf("   ✅ 비상 대응 자동화\n");
    printf("   ✅ 충돌 회피 시스템\n");
    printf("   ✅ 안전 중요 시스템 설계\n");
    
    return 0;
}