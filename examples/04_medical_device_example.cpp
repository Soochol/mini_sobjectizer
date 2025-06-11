// 04_medical_device_example.cpp - 의료기기 환자 모니터링 시스템
// 이 예제는 생명 중요 시스템에서의 실시간 모니터링을 보여줍니다

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace mini_so;

// ============================================================================
// 의료기기 메시지 정의
// ============================================================================
struct VitalSigns {
    uint32_t patient_id;
    uint16_t heart_rate;        // BPM
    uint16_t blood_pressure_sys; // mmHg
    uint16_t blood_pressure_dia; // mmHg
    uint16_t oxygen_saturation; // %
    float temperature;          // °C
    uint16_t respiratory_rate;  // breaths/min
    uint32_t timestamp;
};

struct MedicalAlert {
    enum Severity : uint8_t { 
        NORMAL = 0, 
        ADVISORY = 1, 
        CAUTION = 2, 
        WARNING = 3, 
        CRITICAL = 4 
    } severity;
    
    enum Type : uint8_t {
        HEART_RATE_ABNORMAL,
        BLOOD_PRESSURE_ABNORMAL,
        OXYGEN_LOW,
        TEMPERATURE_ABNORMAL,
        RESPIRATORY_DISTRESS,
        SENSOR_MALFUNCTION,
        BATTERY_LOW,
        SYSTEM_ERROR
    } alert_type;
    
    uint32_t patient_id;
    const char* message;
    float value;
    uint32_t timestamp;
};

struct TreatmentCommand {
    enum Action : uint8_t {
        ADMINISTER_MEDICATION,
        ADJUST_OXYGEN,
        CALL_NURSE,
        CALL_DOCTOR,
        EMERGENCY_RESPONSE,
        INCREASE_MONITORING
    } action;
    
    uint32_t patient_id;
    float dosage_or_level;
    const char* medication_name;
    bool requires_confirmation;
};

struct DeviceStatus {
    uint32_t device_id;
    bool operational;
    uint8_t battery_level;      // %
    uint32_t uptime_hours;
    uint32_t sensor_errors;
    float calibration_drift;
};

struct PatientRecord {
    uint32_t patient_id;
    const char* patient_name;
    uint8_t age;
    const char* condition;
    bool critical_care;
    VitalSigns baseline_vitals;
};

// ============================================================================
// 심박수 모니터링 Agent
// ============================================================================
class HeartRateMonitorAgent : public Agent {
private:
    uint32_t patient_id_;
    uint16_t baseline_hr_;
    uint16_t current_hr_ = 72;
    uint32_t measurement_count_ = 0;
    bool arrhythmia_detected_ = false;
    
public:
    HeartRateMonitorAgent(uint32_t patient_id, uint16_t baseline) 
        : patient_id_(patient_id), baseline_hr_(baseline) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(TreatmentCommand, handle_treatment);
        return false;
    }
    
    void measure_heart_rate() {
        measurement_count_++;
        
        // 심박수 시뮬레이션 (정상: 60-100 BPM)
        float variation = 5.0f * sin(measurement_count_ * 0.1f);
        current_hr_ = baseline_hr_ + static_cast<uint16_t>(variation);
        
        // 부정맥 시뮬레이션 (가끔 발생)
        if (measurement_count_ > 100 && measurement_count_ < 120) {
            current_hr_ = 45 + (measurement_count_ % 10) * 3;  // 서맥
            arrhythmia_detected_ = true;
        } else if (measurement_count_ > 200 && measurement_count_ < 210) {
            current_hr_ = 120 + (measurement_count_ % 15) * 2; // 빈맥
            arrhythmia_detected_ = true;
        } else {
            arrhythmia_detected_ = false;
        }
        
        // 비정상 심박수 감지 및 경고
        MedicalAlert::Severity severity = MedicalAlert::NORMAL;
        if (current_hr_ < 50) {
            severity = MedicalAlert::CRITICAL;
            printf("🚨 심박수 위험: %u BPM (서맥) - 환자 %u\n", current_hr_, patient_id_);
        } else if (current_hr_ > 120) {
            severity = MedicalAlert::WARNING;
            printf("⚠️ 심박수 주의: %u BPM (빈맥) - 환자 %u\n", current_hr_, patient_id_);
        } else if (current_hr_ < 60 || current_hr_ > 100) {
            severity = MedicalAlert::CAUTION;
        }
        
        if (severity > MedicalAlert::NORMAL) {
            MedicalAlert alert{
                severity,
                MedicalAlert::HEART_RATE_ABNORMAL,
                patient_id_,
                arrhythmia_detected_ ? "부정맥 감지됨" : "심박수 비정상",
                static_cast<float>(current_hr_),
                measurement_count_
            };
            MINI_SO_BROADCAST(alert);
        }
        
        // 정상적인 측정값도 주기적으로 전송
        if (measurement_count_ % 10 == 0) {
            printf("💓 심박수: %u BPM (환자 %u, 측정 #%u)\n", 
                   current_hr_, patient_id_, measurement_count_);
        }
        
        MINI_SO_RECORD_PERFORMANCE(50, 1);  // 50μs 처리 시간
        MINI_SO_HEARTBEAT();
    }
    
    uint16_t get_current_heart_rate() const { return current_hr_; }
    
private:
    void handle_treatment(const TreatmentCommand& cmd) {
        if (cmd.patient_id != patient_id_) return;
        
        if (cmd.action == TreatmentCommand::INCREASE_MONITORING) {
            printf("📈 심박수 모니터: 모니터링 주기 증가 - 환자 %u\n", patient_id_);
        }
    }
};

// ============================================================================
// 혈압 모니터링 Agent
// ============================================================================
class BloodPressureMonitorAgent : public Agent {
private:
    uint32_t patient_id_;
    uint16_t baseline_sys_, baseline_dia_;
    uint16_t current_sys_ = 120, current_dia_ = 80;
    uint32_t measurement_count_ = 0;
    
public:
    BloodPressureMonitorAgent(uint32_t patient_id, uint16_t sys, uint16_t dia)
        : patient_id_(patient_id), baseline_sys_(sys), baseline_dia_(dia) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        (void)msg;  // 경고 방지
        return false;  // 독립적으로 동작
    }
    
    void measure_blood_pressure() {
        measurement_count_++;
        
        // 혈압 시뮬레이션 (정상: 수축기 90-140, 이완기 60-90)
        float sys_variation = 10.0f * cos(measurement_count_ * 0.08f);
        float dia_variation = 8.0f * sin(measurement_count_ * 0.12f);
        
        current_sys_ = baseline_sys_ + static_cast<uint16_t>(sys_variation);
        current_dia_ = baseline_dia_ + static_cast<uint16_t>(dia_variation);
        
        // 고혈압 에피소드 시뮬레이션
        if (measurement_count_ > 150 && measurement_count_ < 170) {
            current_sys_ = 160 + (measurement_count_ % 8) * 2;
            current_dia_ = 95 + (measurement_count_ % 5) * 2;
        }
        
        // 혈압 평가
        MedicalAlert::Severity severity = MedicalAlert::NORMAL;
        const char* condition = "정상";
        
        if (current_sys_ >= 180 || current_dia_ >= 110) {
            severity = MedicalAlert::CRITICAL;
            condition = "고혈압 위기";
        } else if (current_sys_ >= 160 || current_dia_ >= 100) {
            severity = MedicalAlert::WARNING;
            condition = "2단계 고혈압";
        } else if (current_sys_ >= 140 || current_dia_ >= 90) {
            severity = MedicalAlert::CAUTION;
            condition = "1단계 고혈압";
        } else if (current_sys_ < 90 || current_dia_ < 60) {
            severity = MedicalAlert::WARNING;
            condition = "저혈압";
        }
        
        if (severity > MedicalAlert::NORMAL) {
            printf("🩸 혈압 경고: %u/%u mmHg (%s) - 환자 %u\n",
                   current_sys_, current_dia_, condition, patient_id_);
                   
            MedicalAlert alert{
                severity,
                MedicalAlert::BLOOD_PRESSURE_ABNORMAL,
                patient_id_,
                condition,
                static_cast<float>(current_sys_),
                measurement_count_
            };
            MINI_SO_BROADCAST(alert);
        }
        
        // 정상 측정값 주기적 출력
        if (measurement_count_ % 15 == 0) {
            printf("🩸 혈압: %u/%u mmHg (환자 %u)\n", 
                   current_sys_, current_dia_, patient_id_);
        }
        
        MINI_SO_RECORD_PERFORMANCE(80, 1);  // 80μs 처리 시간
        if (measurement_count_ % 3 == 0) {
            MINI_SO_HEARTBEAT();
        }
    }
};

// ============================================================================
// 산소포화도 모니터링 Agent
// ============================================================================
class OxygenSaturationAgent : public Agent {
private:
    uint32_t patient_id_;
    uint16_t current_spo2_ = 98;
    uint32_t measurement_count_ = 0;
    bool hypoxia_alert_active_ = false;
    
public:
    OxygenSaturationAgent(uint32_t patient_id) : patient_id_(patient_id) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(TreatmentCommand, handle_oxygen_treatment);
        return false;
    }
    
    void measure_oxygen_saturation() {
        measurement_count_++;
        
        // 산소포화도 시뮬레이션 (정상: 95-100%)
        float variation = 2.0f * sin(measurement_count_ * 0.15f);
        current_spo2_ = 98 + static_cast<uint16_t>(variation);
        
        // 저산소증 에피소드 시뮬레이션
        if (measurement_count_ > 80 && measurement_count_ < 100) {
            current_spo2_ = 88 - (measurement_count_ - 80) / 2;
            if (current_spo2_ < 85) current_spo2_ = 85;
        }
        
        // 산소포화도 평가
        MedicalAlert::Severity severity = MedicalAlert::NORMAL;
        const char* condition = "정상";
        bool new_hypoxia_alert = false;
        
        if (current_spo2_ < 85) {
            severity = MedicalAlert::CRITICAL;
            condition = "심각한 저산소증";
            new_hypoxia_alert = true;
        } else if (current_spo2_ < 90) {
            severity = MedicalAlert::WARNING;
            condition = "중등도 저산소증";
            new_hypoxia_alert = true;
        } else if (current_spo2_ < 95) {
            severity = MedicalAlert::CAUTION;
            condition = "경미한 저산소증";
        }
        
        // 새로운 저산소증 알림 또는 회복 알림
        if (new_hypoxia_alert && !hypoxia_alert_active_) {
            printf("🫁 산소포화도 위험: %u%% (%s) - 환자 %u\n",
                   current_spo2_, condition, patient_id_);
                   
            MedicalAlert alert{
                severity,
                MedicalAlert::OXYGEN_LOW,
                patient_id_,
                condition,
                static_cast<float>(current_spo2_),
                measurement_count_
            };
            MINI_SO_BROADCAST(alert);
            
            // 자동 산소 공급 증가 요청
            if (severity >= MedicalAlert::WARNING) {
                TreatmentCommand oxygen_cmd{
                    TreatmentCommand::ADJUST_OXYGEN,
                    patient_id_,
                    4.0f,  // 4L/min
                    "Oxygen",
                    false
                };
                MINI_SO_BROADCAST(oxygen_cmd);
            }
            
        } else if (!new_hypoxia_alert && hypoxia_alert_active_) {
            printf("✅ 산소포화도 회복: %u%% - 환자 %u\n", current_spo2_, patient_id_);
        }
        
        hypoxia_alert_active_ = new_hypoxia_alert;
        
        // 정상 측정값 주기적 출력
        if (measurement_count_ % 12 == 0) {
            printf("🫁 산소포화도: %u%% (환자 %u)\n", current_spo2_, patient_id_);
        }
        
        MINI_SO_RECORD_PERFORMANCE(60, 1);  // 60μs 처리 시간
        MINI_SO_HEARTBEAT();
    }
    
private:
    void handle_oxygen_treatment(const TreatmentCommand& cmd) {
        if (cmd.patient_id != patient_id_) return;
        
        if (cmd.action == TreatmentCommand::ADJUST_OXYGEN) {
            printf("💨 산소 공급 조절: %.1fL/min - 환자 %u\n", 
                   cmd.dosage_or_level, patient_id_);
        }
    }
};

// ============================================================================
// 중앙 모니터링 및 알림 Agent
// ============================================================================
class CentralMonitoringAgent : public Agent {
private:
    uint32_t total_alerts_ = 0;
    uint32_t critical_alerts_ = 0;
    uint32_t treatments_administered_ = 0;
    uint32_t patient_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(MedicalAlert, handle_medical_alert);
        HANDLE_MESSAGE_VOID(VitalSigns, log_vital_signs);
        return false;
    }
    
    void print_monitoring_summary() {
        printf("\n🏥 중앙 모니터링 요약:\n");
        printf("   - 총 경고: %u회\n", total_alerts_);
        printf("   - 중요 경고: %u회\n", critical_alerts_);
        printf("   - 처치 시행: %u회\n", treatments_administered_);
        printf("   - 모니터링 환자: %u명\n", patient_count_);
        
        float alert_rate = patient_count_ > 0 ? 
            (float)total_alerts_ / patient_count_ : 0.0f;
        
        if (critical_alerts_ == 0) {
            printf("   ✅ 중요 사건 없음 - 안정적 모니터링\n");
        } else if (critical_alerts_ < 3) {
            printf("   ⚠️ 일부 중요 사건 발생 - 지속 관찰 필요\n");
        } else {
            printf("   🚨 다수 중요 사건 - 의료진 추가 투입 필요\n");
        }
        
        printf("   - 평균 경고율: %.1f회/환자\n", alert_rate);
    }
    
private:
    void handle_medical_alert(const MedicalAlert& alert) {
        total_alerts_++;
        
        const char* severity_str = "";
        const char* alert_type_str = "";
        
        switch (alert.severity) {
            case MedicalAlert::NORMAL: severity_str = "정상"; break;
            case MedicalAlert::ADVISORY: severity_str = "참고"; break;
            case MedicalAlert::CAUTION: severity_str = "주의"; break;
            case MedicalAlert::WARNING: severity_str = "경고"; break;
            case MedicalAlert::CRITICAL: 
                severity_str = "위험"; 
                critical_alerts_++;
                break;
        }
        
        switch (alert.alert_type) {
            case MedicalAlert::HEART_RATE_ABNORMAL: alert_type_str = "심박수 이상"; break;
            case MedicalAlert::BLOOD_PRESSURE_ABNORMAL: alert_type_str = "혈압 이상"; break;
            case MedicalAlert::OXYGEN_LOW: alert_type_str = "산소포화도 저하"; break;
            case MedicalAlert::TEMPERATURE_ABNORMAL: alert_type_str = "체온 이상"; break;
            case MedicalAlert::RESPIRATORY_DISTRESS: alert_type_str = "호흡 곤란"; break;
            case MedicalAlert::SENSOR_MALFUNCTION: alert_type_str = "센서 오류"; break;
            case MedicalAlert::BATTERY_LOW: alert_type_str = "배터리 부족"; break;
            case MedicalAlert::SYSTEM_ERROR: alert_type_str = "시스템 오류"; break;
        }
        
        printf("🏥 중앙모니터 [%s]: %s - %s (환자 %u, 값: %.1f)\n",
               severity_str, alert_type_str, alert.message, 
               alert.patient_id, alert.value);
        
        // 중요 경고 시 자동 대응
        if (alert.severity >= MedicalAlert::WARNING) {
            TreatmentCommand response;
            response.patient_id = alert.patient_id;
            response.requires_confirmation = (alert.severity == MedicalAlert::CRITICAL);
            
            switch (alert.alert_type) {
                case MedicalAlert::HEART_RATE_ABNORMAL:
                    response.action = TreatmentCommand::CALL_DOCTOR;
                    strcpy(const_cast<char*>(response.medication_name), "Cardiologist");
                    break;
                case MedicalAlert::OXYGEN_LOW:
                    response.action = TreatmentCommand::ADJUST_OXYGEN;
                    response.dosage_or_level = 6.0f;
                    strcpy(const_cast<char*>(response.medication_name), "Oxygen");
                    break;
                case MedicalAlert::BLOOD_PRESSURE_ABNORMAL:
                    response.action = TreatmentCommand::CALL_NURSE;
                    strcpy(const_cast<char*>(response.medication_name), "BP Specialist");
                    break;
                default:
                    response.action = TreatmentCommand::INCREASE_MONITORING;
                    break;
            }
            
            MINI_SO_BROADCAST(response);
            treatments_administered_++;
        }
        
        MINI_SO_HEARTBEAT();
    }
    
    void log_vital_signs(const VitalSigns& vitals) {
        // 새 환자 등록 확인
        static uint32_t known_patients[10] = {0};
        bool new_patient = true;
        
        for (int i = 0; i < 10; ++i) {
            if (known_patients[i] == vitals.patient_id) {
                new_patient = false;
                break;
            }
            if (known_patients[i] == 0) {
                known_patients[i] = vitals.patient_id;
                new_patient = true;
                patient_count_++;
                break;
            }
        }
        
        if (new_patient) {
            printf("📋 새 환자 등록: ID %u\n", vitals.patient_id);
        }
    }
};

// ============================================================================
// 메인 함수: 의료기기 모니터링 시뮬레이션
// ============================================================================
int main() {
    printf("🏥 의료기기 환자 모니터링 시스템 🏥\n");
    printf("================================\n\n");
    
    // 시스템 초기화
    MINI_SO_INIT();
    if (!sys.initialize()) {
        printf("❌ 의료 시스템 초기화 실패!\n");
        return 1;
    }
    printf("✅ 의료 모니터링 시스템 초기화 완료\n\n");
    
    // 환자별 모니터링 Agent 생성
    HeartRateMonitorAgent hr_monitor_p1(101, 72);    // 환자 101, 기준 심박수 72
    BloodPressureMonitorAgent bp_monitor_p1(101, 120, 80);  // 120/80 기준
    OxygenSaturationAgent spo2_monitor_p1(101);
    
    HeartRateMonitorAgent hr_monitor_p2(102, 68);    // 환자 102, 기준 심박수 68
    BloodPressureMonitorAgent bp_monitor_p2(102, 110, 75);  // 110/75 기준
    OxygenSaturationAgent spo2_monitor_p2(102);
    
    // 중앙 모니터링 Agent
    CentralMonitoringAgent central_monitor;
    
    // Agent 등록
    MINI_SO_REGISTER(hr_monitor_p1);
    MINI_SO_REGISTER(bp_monitor_p1);
    MINI_SO_REGISTER(spo2_monitor_p1);
    MINI_SO_REGISTER(hr_monitor_p2);
    MINI_SO_REGISTER(bp_monitor_p2);
    MINI_SO_REGISTER(spo2_monitor_p2);
    MINI_SO_REGISTER(central_monitor);
    
    printf("🔧 환자 모니터링 장비 활성화:\n");
    printf("   - 환자 101: 심박수, 혈압, 산소포화도 모니터링\n");
    printf("   - 환자 102: 심박수, 혈압, 산소포화도 모니터링\n");
    printf("   - 중앙 모니터링 시스템 활성화\n\n");
    
    // 환자 기본 정보 전송
    VitalSigns patient1_baseline{101, 72, 120, 80, 98, 36.5f, 16, 0};
    VitalSigns patient2_baseline{102, 68, 110, 75, 97, 36.8f, 18, 0};
    
    env.send_message(7, 7, patient1_baseline);  // central_monitor에게
    env.send_message(7, 7, patient2_baseline);
    
    printf("🚀 환자 모니터링 시작!\n");
    printf("==========================================\n");
    
    // 모니터링 시뮬레이션 실행
    for (int cycle = 0; cycle < 50; ++cycle) {
        printf("\n--- 모니터링 사이클 %d (%.1f분) ---\n", 
               cycle + 1, cycle * 0.2f);  // 12초마다 = 0.2분
        
        // 환자 1 바이탈 사인 측정
        hr_monitor_p1.measure_heart_rate();
        if (cycle % 3 == 0) {  // 혈압은 덜 자주 측정
            bp_monitor_p1.measure_blood_pressure();
        }
        spo2_monitor_p1.measure_oxygen_saturation();
        
        // 환자 2 바이탈 사인 측정
        hr_monitor_p2.measure_heart_rate();
        if (cycle % 3 == 1) {  // 혈압 측정 시점을 다르게
            bp_monitor_p2.measure_blood_pressure();
        }
        spo2_monitor_p2.measure_oxygen_saturation();
        
        // 모든 메시지 처리
        MINI_SO_RUN();
        
        // 주기적 상태 확인
        if (cycle % 20 == 19) {
            printf("\n📊 시스템 상태 점검 (%.1f분):\n", cycle * 0.2f);
            printf("   - 의료 시스템 상태: %s\n", sys.is_healthy() ? "정상" : "점검 필요");
            printf("   - 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
        }
    }
    
    printf("\n==========================================\n");
    printf("📊 모니터링 세션 완료\n");
    printf("==========================================\n");
    
    central_monitor.print_monitoring_summary();
    
    printf("\n📈 의료 시스템 성능:\n");
    printf("   - 총 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("   - 최대 처리 시간: %u μs\n", sys.performance().max_processing_time_us());
    printf("   - 시스템 에러: %zu\n", sys.error().error_count());
    printf("   - 시스템 신뢰도: %s\n", sys.is_healthy() ? "매우 높음" : "점검 필요");
    
    printf("\n🎯 의료기기 모니터링 시뮬레이션 완료!\n");
    printf("💡 이 예제에서 배운 내용:\n");
    printf("   ✅ 실시간 생체 신호 모니터링\n");
    printf("   ✅ 다중 환자 동시 모니터링\n");
    printf("   ✅ 의료 경고 시스템\n");
    printf("   ✅ 자동 치료 대응 시스템\n");
    printf("   ✅ 생명 중요 시스템 안전성\n");
    printf("   ✅ 의료진 알림 자동화\n");
    
    return 0;
}