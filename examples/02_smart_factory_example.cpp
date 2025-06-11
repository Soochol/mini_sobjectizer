// 02_smart_factory_example.cpp - 스마트 팩토리 생산 라인 시뮬레이션
// 이 예제는 실시간 제조 환경에서의 Actor Model 사용을 보여줍니다

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>
#include <cmath>

using namespace mini_so;

// ============================================================================
// 스마트 팩토리 메시지 정의
// ============================================================================
struct SensorReading {
    enum Type : uint8_t { TEMPERATURE, PRESSURE, VIBRATION, FLOW_RATE } type;
    float value;
    uint32_t sensor_id;
    uint32_t timestamp;
    bool alert_level;  // 임계값 초과 여부
};

struct ProductionCommand {
    enum Type : uint8_t { START_LINE, STOP_LINE, ADJUST_SPEED, EMERGENCY_STOP } command;
    uint32_t line_id;
    float parameter;  // 속도, 온도 등
};

struct QualityReport {
    uint32_t product_id;
    bool passed;
    float quality_score;
    const char* defect_reason;
};

struct MaintenanceAlert {
    enum Priority : uint8_t { LOW, MEDIUM, HIGH, CRITICAL } priority;
    uint32_t equipment_id;
    const char* issue_description;
    float predicted_failure_time_hours;
};

struct ProductionMetrics {
    uint32_t line_id;
    uint32_t products_per_hour;
    float efficiency_percent;
    uint32_t defect_count;
    float uptime_percent;
};

// ============================================================================
// 센서 모니터링 Agent
// ============================================================================
class SensorMonitorAgent : public Agent {
private:
    uint32_t sensor_id_;
    SensorReading::Type sensor_type_;
    float current_value_ = 0.0f;
    float threshold_min_, threshold_max_;
    uint32_t reading_count_ = 0;
    bool alert_active_ = false;
    
public:
    SensorMonitorAgent(uint32_t sensor_id, SensorReading::Type type, 
                      float min_threshold, float max_threshold)
        : sensor_id_(sensor_id), sensor_type_(type), 
          threshold_min_(min_threshold), threshold_max_(max_threshold) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(ProductionCommand, handle_production_command);
        return false;
    }
    
    void take_reading() {
        reading_count_++;
        
        // 센서 타입별 시뮬레이션 데이터 생성
        switch (sensor_type_) {
            case SensorReading::TEMPERATURE:
                current_value_ = 22.0f + 3.0f * sin(reading_count_ * 0.1f) + 
                               (reading_count_ % 10) * 0.5f;
                break;
            case SensorReading::PRESSURE:
                current_value_ = 1.0f + 0.2f * cos(reading_count_ * 0.15f) + 
                               (reading_count_ % 8) * 0.02f;
                break;
            case SensorReading::VIBRATION:
                current_value_ = 0.5f + 0.3f * sin(reading_count_ * 0.2f) + 
                               (reading_count_ % 15) * 0.1f;
                break;
            case SensorReading::FLOW_RATE:
                current_value_ = 50.0f + 10.0f * cos(reading_count_ * 0.08f) + 
                               (reading_count_ % 12) * 0.8f;
                break;
        }
        
        // 임계값 확인
        bool new_alert = (current_value_ < threshold_min_ || current_value_ > threshold_max_);
        
        // 알림 상태 변경 시 특별 처리
        if (new_alert != alert_active_) {
            alert_active_ = new_alert;
            if (alert_active_) {
                printf("🚨 ALERT: Sensor %u (%s) = %.2f (임계값: %.2f-%.2f)\n",
                       sensor_id_, get_sensor_type_name(), current_value_,
                       threshold_min_, threshold_max_);
            } else {
                printf("✅ NORMAL: Sensor %u (%s) = %.2f\n",
                       sensor_id_, get_sensor_type_name(), current_value_);
            }
        }
        
        // 센서 데이터 전송
        SensorReading reading{
            sensor_type_, current_value_, sensor_id_, 
            static_cast<uint32_t>(reading_count_), alert_active_
        };
        MINI_SO_BROADCAST(reading);
        
        // 주기적 heartbeat
        if (reading_count_ % 5 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
    
private:
    const char* get_sensor_type_name() const {
        switch (sensor_type_) {
            case SensorReading::TEMPERATURE: return "Temperature";
            case SensorReading::PRESSURE: return "Pressure";
            case SensorReading::VIBRATION: return "Vibration";
            case SensorReading::FLOW_RATE: return "Flow Rate";
            default: return "Unknown";
        }
    }
    
    void handle_production_command(const ProductionCommand& cmd) {
        if (cmd.command == ProductionCommand::EMERGENCY_STOP) {
            printf("🛑 Sensor %u: Emergency stop received - halting readings\n", sensor_id_);
            alert_active_ = false;
        }
    }
};

// ============================================================================
// 생산 라인 제어 Agent
// ============================================================================
class ProductionLineAgent : public Agent {
private:
    uint32_t line_id_;
    bool is_running_ = false;
    float line_speed_ = 100.0f;  // products per hour
    uint32_t products_produced_ = 0;
    uint32_t alert_count_ = 0;
    float efficiency_ = 95.0f;
    
public:
    ProductionLineAgent(uint32_t line_id) : line_id_(line_id) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(SensorReading, handle_sensor_reading);
        HANDLE_MESSAGE_VOID(ProductionCommand, handle_production_command);
        HANDLE_MESSAGE_VOID(QualityReport, handle_quality_report);
        return false;
    }
    
    void run_production_cycle() {
        if (!is_running_) return;
        
        products_produced_++;
        
        // 품질 검사 시뮬레이션
        float quality_score = 0.85f + 0.15f * (rand() % 100) / 100.0f;
        bool passed = quality_score > 0.8f && alert_count_ == 0;
        
        QualityReport quality{
            products_produced_, 
            passed, 
            quality_score, 
            passed ? "OK" : (alert_count_ > 0 ? "Sensor Alert" : "Quality Issue")
        };
        MINI_SO_BROADCAST(quality);
        
        // 생산 메트릭 주기적 전송
        if (products_produced_ % 10 == 0) {
            ProductionMetrics metrics{
                line_id_, 
                static_cast<uint32_t>(line_speed_),
                efficiency_,
                products_produced_ - (products_produced_ * passed),
                is_running_ ? 98.5f : 0.0f
            };
            MINI_SO_BROADCAST(metrics);
            
            printf("📊 Line %u: %u products, %.1f%% efficiency, Speed: %.1f/hr\n",
                   line_id_, products_produced_, efficiency_, line_speed_);
        }
        
        MINI_SO_HEARTBEAT();
    }
    
private:
    void handle_sensor_reading(const SensorReading& reading) {
        if (reading.alert_level) {
            alert_count_++;
            
            // 온도 알림 시 자동 속도 조절
            if (reading.type == SensorReading::TEMPERATURE && reading.value > 28.0f) {
                line_speed_ = std::max(50.0f, line_speed_ * 0.8f);
                printf("🌡️ Line %u: High temperature (%.1f°C) - reducing speed to %.1f\n",
                       line_id_, reading.value, line_speed_);
            }
            
            // 진동 알림 시 유지보수 요청
            if (reading.type == SensorReading::VIBRATION && reading.value > 1.0f) {
                MaintenanceAlert maint{
                    MaintenanceAlert::HIGH, 
                    reading.sensor_id,
                    "High vibration detected - check bearings",
                    24.0f  // 24시간 내 점검 필요
                };
                MINI_SO_BROADCAST(maint);
            }
        } else if (alert_count_ > 0) {
            alert_count_--;  // 정상화 시 알림 카운트 감소
            if (alert_count_ == 0) {
                line_speed_ = std::min(120.0f, line_speed_ * 1.1f);  // 속도 복구
            }
        }
    }
    
    void handle_production_command(const ProductionCommand& cmd) {
        if (cmd.line_id != line_id_ && cmd.line_id != 0) return;  // 0은 전체 라인
        
        switch (cmd.command) {
            case ProductionCommand::START_LINE:
                is_running_ = true;
                printf("🚀 Line %u: Production started\n", line_id_);
                break;
            case ProductionCommand::STOP_LINE:
                is_running_ = false;
                printf("⏹️ Line %u: Production stopped\n", line_id_);
                break;
            case ProductionCommand::ADJUST_SPEED:
                line_speed_ = cmd.parameter;
                printf("⚙️ Line %u: Speed adjusted to %.1f products/hour\n", 
                       line_id_, line_speed_);
                break;
            case ProductionCommand::EMERGENCY_STOP:
                is_running_ = false;
                line_speed_ = 0.0f;
                printf("🚨 Line %u: EMERGENCY STOP activated!\n", line_id_);
                break;
        }
    }
    
    void handle_quality_report(const QualityReport& report) {
        if (!report.passed) {
            efficiency_ = std::max(70.0f, efficiency_ - 0.5f);
        } else {
            efficiency_ = std::min(99.0f, efficiency_ + 0.1f);
        }
    }
};

// ============================================================================
// 품질 관리 Agent
// ============================================================================
class QualityControlAgent : public Agent {
private:
    uint32_t total_products_ = 0;
    uint32_t passed_products_ = 0;
    uint32_t defective_products_ = 0;
    float average_quality_ = 0.0f;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(QualityReport, process_quality_report);
        return false;
    }
    
    void print_quality_summary() {
        float pass_rate = total_products_ > 0 ? 
            (100.0f * passed_products_ / total_products_) : 0.0f;
        
        printf("\n📋 품질 관리 요약:\n");
        printf("   - 총 생산품: %u\n", total_products_);
        printf("   - 합격품: %u (%.1f%%)\n", passed_products_, pass_rate);
        printf("   - 불량품: %u\n", defective_products_);
        printf("   - 평균 품질: %.2f\n", average_quality_);
        
        if (pass_rate < 90.0f) {
            printf("   ⚠️ 품질 개선 필요 (목표: 95%%)\n");
        } else {
            printf("   ✅ 품질 기준 충족\n");
        }
    }
    
private:
    void process_quality_report(const QualityReport& report) {
        total_products_++;
        
        // 이동 평균으로 품질 점수 계산
        average_quality_ = (average_quality_ * (total_products_ - 1) + report.quality_score) 
                          / total_products_;
        
        if (report.passed) {
            passed_products_++;
        } else {
            defective_products_++;
            printf("❌ Quality: Product %u failed - %s (Score: %.2f)\n",
                   report.product_id, report.defect_reason, report.quality_score);
        }
        
        // 주기적 품질 보고
        if (total_products_ % 20 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
};

// ============================================================================
// 유지보수 관리 Agent
// ============================================================================
class MaintenanceAgent : public Agent {
private:
    uint32_t maintenance_requests_ = 0;
    uint32_t critical_alerts_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(MaintenanceAlert, handle_maintenance_alert);
        return false;
    }
    
private:
    void handle_maintenance_alert(const MaintenanceAlert& alert) {
        maintenance_requests_++;
        
        const char* priority_str = "";
        switch (alert.priority) {
            case MaintenanceAlert::LOW: priority_str = "낮음"; break;
            case MaintenanceAlert::MEDIUM: priority_str = "보통"; break;
            case MaintenanceAlert::HIGH: priority_str = "높음"; break;
            case MaintenanceAlert::CRITICAL: 
                priority_str = "긴급"; 
                critical_alerts_++;
                // 긴급 정지 명령 전송
                ProductionCommand emergency{ProductionCommand::EMERGENCY_STOP, 0, 0.0f};
                MINI_SO_BROADCAST(emergency);
                break;
        }
        
        printf("🔧 유지보수 요청 #%u [%s]: 장비 %u - %s (예상 시간: %.1fh)\n",
               maintenance_requests_, priority_str, alert.equipment_id,
               alert.issue_description, alert.predicted_failure_time_hours);
        
        MINI_SO_HEARTBEAT();
    }
};

// ============================================================================
// 메인 함수: 스마트 팩토리 시뮬레이션
// ============================================================================
int main() {
    printf("🏭 스마트 팩토리 생산 라인 시뮬레이션 🏭\n");
    printf("=====================================\n\n");
    
    // 시스템 초기화
    MINI_SO_INIT();
    if (!sys.initialize()) {
        printf("❌ 시스템 초기화 실패!\n");
        return 1;
    }
    printf("✅ 팩토리 제어 시스템 초기화 완료\n\n");
    
    // 센서 Agent들 생성
    SensorMonitorAgent temp_sensor(101, SensorReading::TEMPERATURE, 18.0f, 30.0f);
    SensorMonitorAgent pressure_sensor(102, SensorReading::PRESSURE, 0.8f, 1.5f);
    SensorMonitorAgent vibration_sensor(103, SensorReading::VIBRATION, 0.0f, 1.2f);
    SensorMonitorAgent flow_sensor(104, SensorReading::FLOW_RATE, 40.0f, 80.0f);
    
    // 생산 라인 Agent들
    ProductionLineAgent line1(1);
    ProductionLineAgent line2(2);
    
    // 관리 Agent들
    QualityControlAgent quality_control;
    MaintenanceAgent maintenance;
    
    // Agent 등록
    MINI_SO_REGISTER(temp_sensor);
    MINI_SO_REGISTER(pressure_sensor);
    MINI_SO_REGISTER(vibration_sensor);
    MINI_SO_REGISTER(flow_sensor);
    AgentId line1_id = MINI_SO_REGISTER(line1);
    AgentId line2_id = MINI_SO_REGISTER(line2);
    MINI_SO_REGISTER(quality_control);
    MINI_SO_REGISTER(maintenance);
    
    printf("🔧 생산 장비 등록 완료 (센서 4개, 라인 2개, 관리 시스템 2개)\n\n");
    
    // 생산 시작 명령
    ProductionCommand start_cmd{ProductionCommand::START_LINE, 0, 100.0f};
    env.send_message(line1_id, line1_id, start_cmd);  // line1에 전송
    env.send_message(line2_id, line2_id, start_cmd);  // line2에 전송
    
    printf("🚀 생산 라인 시작!\n");
    printf("==========================================\n");
    
    // 생산 시뮬레이션 실행
    for (int cycle = 0; cycle < 30; ++cycle) {
        printf("\n--- 생산 사이클 %d ---\n", cycle + 1);
        
        // 센서 읽기
        temp_sensor.take_reading();
        pressure_sensor.take_reading();
        vibration_sensor.take_reading();
        flow_sensor.take_reading();
        
        // 생산 실행
        line1.run_production_cycle();
        line2.run_production_cycle();
        
        // 메시지 처리
        MINI_SO_RUN();
        
        // 주기적 상태 확인
        if (cycle % 10 == 9) {
            printf("\n📊 중간 상태 점검 (사이클 %d):\n", cycle + 1);
            quality_control.print_quality_summary();
            printf("   - 시스템 상태: %s\n", sys.is_healthy() ? "정상" : "점검 필요");
        }
        
        // 시뮬레이션 속도 조절을 위한 간단한 지연
        if (cycle == 15) {
            printf("\n⚙️ 중간 속도 조절 시뮬레이션\n");
            ProductionCommand adjust{ProductionCommand::ADJUST_SPEED, 1, 80.0f};
            env.send_message(line1_id, line1_id, adjust);
        }
    }
    
    printf("\n==========================================\n");
    printf("📊 최종 생산 보고서\n");
    printf("==========================================\n");
    
    quality_control.print_quality_summary();
    
    printf("\n📈 시스템 성능:\n");
    printf("   - 총 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("   - 최대 처리 시간: %u μs\n", sys.performance().max_processing_time_us());
    printf("   - 시스템 에러: %zu\n", sys.error().error_count());
    
    printf("\n🎯 스마트 팩토리 시뮬레이션 완료!\n");
    printf("💡 이 예제에서 배운 내용:\n");
    printf("   ✅ 다중 센서 모니터링\n");
    printf("   ✅ 실시간 생산 라인 제어\n");
    printf("   ✅ 품질 관리 자동화\n");
    printf("   ✅ 예측 유지보수\n");
    printf("   ✅ 비상 정지 시스템\n");
    
    return 0;
}