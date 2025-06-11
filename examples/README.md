# Mini SObjectizer Examples

이 디렉토리는 Mini SObjectizer v3.0의 다양한 실전 예제를 포함합니다. 각 예제는 서로 다른 응용 분야에서 Actor Model의 활용법을 보여줍니다.

## 🎓 학습 순서 (권장)

### 1. [01_beginner_tutorial.cpp](01_beginner_tutorial.cpp)
**초보자용 완전한 튜토리얼**
- Mini SObjectizer의 기본 개념 설명
- User-friendly 매크로 사용법
- 단계별 Agent 구현
- 메시지 처리 흐름 이해

**학습 내용:**
- `MINI_SO_INIT()` - 시스템 초기화
- `MINI_SO_REGISTER()` - Agent 등록
- `BEGIN_MESSAGE_HANDLER()` / `END_MESSAGE_HANDLER()` - 핸들러 선언 간소화
- `HANDLE_MESSAGE_AUTO()` - 자동 반환 타입 감지 (VOID 불필요!)
- `SEND_TO_SELF()` - 자신에게 메시지 전송
- `DEFINE_AGENT()` / `END_AGENT()` - Agent 클래스 정의 간소화
- `MINI_SO_BROADCAST()` - 메시지 전송
- `MINI_SO_HEARTBEAT()` - 상태 보고

---

## 🏭 산업 자동화 예제

### 2. [02_smart_factory_example.cpp](02_smart_factory_example.cpp)
**스마트 팩토리 생산 라인 시뮬레이션**
- 실시간 센서 모니터링 (온도, 압력, 진동, 유량)
- 생산 라인 자동 제어
- 품질 관리 시스템
- 예측 유지보수
- 비상 정지 시스템

**핵심 기술:**
- 다중 센서 데이터 융합
- 실시간 알림 시스템
- 자동 속도 조절
- 품질 통계 분석

**메시지 타입:**
- `SensorReading` - 센서 데이터
- `ProductionCommand` - 생산 제어
- `QualityReport` - 품질 보고
- `MaintenanceAlert` - 유지보수 알림

---

## 🚗 자율주행 및 모빌리티

### 3. [03_autonomous_vehicle_example.cpp](03_autonomous_vehicle_example.cpp)
**자율주행 차량 센서 융합 시스템**
- LiDAR, Camera, Radar 센서 통합
- 실시간 장애물 감지
- 충돌 회피 시스템
- 안전 중요 시스템 설계
- 비상 브레이킹 자동화

**핵심 기술:**
- 센서 융합 알고리즘
- 실시간 안전 판단
- TTC (Time To Collision) 계산
- 다중 센서 검증

**안전 기능:**
- 보행자 감지 및 경고
- 차선 이탈 감지
- 자동 긴급 제동
- 다단계 위험도 평가

---

## 🏥 의료 및 헬스케어

### 4. [04_medical_device_example.cpp](04_medical_device_example.cpp)
**의료기기 환자 모니터링 시스템**
- 다중 환자 동시 모니터링
- 실시간 생체 신호 감시
- 자동 의료 경고 시스템
- 의료진 알림 자동화
- 생명 중요 시스템 신뢰성

**모니터링 항목:**
- 심박수 (부정맥 감지)
- 혈압 (고혈압/저혈압)
- 산소포화도 (저산소증)
- 자동 산소 공급 조절

**안전 기능:**
- 실시간 위험도 평가
- 자동 치료 대응
- 의료진 호출 시스템
- 환자별 맞춤 모니터링

---

## 🚁 항공 및 제어 시스템

### 5. [05_drone_control_example.cpp](05_drone_control_example.cpp)
**드론 비행 제어 시스템**
- IMU + GPS 센서 융합
- 실시간 다축 비행 제어
- PID 제어기 구현
- 자동 이착륙 시스템
- 배터리 관리 및 안전 착륙

**제어 기능:**
- 6축 자세 제어 (Roll, Pitch, Yaw)
- 고도 및 위치 제어
- 자동 호버링
- RTH (Return To Home)
- 비상 착륙

**센서 시스템:**
- IMU (가속도계, 자이로, 지자기)
- GPS 위치 추적
- 배터리 모니터링
- 실시간 상태 머신

---

## 🚀 고급 매크로 활용

### 6. [06_advanced_macros_example.cpp](06_advanced_macros_example.cpp)
**새로운 고급 매크로 시스템 데모**
- 메시지 핸들러 선언 간소화
- 자동 반환 타입 감지
- 자신에게 메시지 전송
- Agent 클래스 정의 간소화
- 조건부 메시지 처리

**새 매크로들:**
- `BEGIN_MESSAGE_HANDLER()` / `END_MESSAGE_HANDLER()` - 핸들러 선언
- `HANDLE_MESSAGE_AUTO()` - VOID 구분 불필요
- `SEND_TO_SELF()` - 자신에게 메시지 전송
- `DEFINE_AGENT()` / `END_AGENT()` - 클래스 정의 간소화
- `HANDLE_MESSAGE_IF()` - 조건부 처리

**Before & After 비교:**
```cpp
// Before (기존)
class MyAgent : public Agent {
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(MyMessage, handle_my_message);
        return false;
    }
private:
    void handle_my_message(const MyMessage& msg) { ... }
};

// After (새 매크로)
DEFINE_AGENT(MyAgent)
    HANDLE_MESSAGE_AUTO(MyMessage, handle_my_message);
private:
    void handle_my_message(const MyMessage& msg) { ... }
END_AGENT()
```

---

## 🔧 빌드 및 실행

### CMake 빌드
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# 개별 예제 실행
./examples/01_beginner_tutorial
./examples/02_smart_factory_example
./examples/03_autonomous_vehicle_example
./examples/04_medical_device_example
./examples/05_drone_control_example
./examples/06_advanced_macros_example
```

### 단일 예제 컴파일
```bash
# 예제 하나만 컴파일하려면:
g++ -std=c++17 -I../include -DUNIT_TEST \
    01_beginner_tutorial.cpp \
    ../src/mini_sobjectizer_final.cpp \
    ../src/freertos_mock.cpp \
    -o beginner_tutorial

./beginner_tutorial
```

---

## 📊 성능 특성

| 예제 | 메시지/초 | Agent 수 | 메모리 사용량 | 특징 |
|------|-----------|----------|---------------|------|
| 초보자 튜토리얼 | ~100 | 3 | 최소 | 학습용 |
| 스마트 팩토리 | ~500 | 8 | 중간 | 산업용 |
| 자율주행 | ~1000 | 5 | 중간 | 안전 중요 |
| 의료 모니터링 | ~800 | 7 | 중간 | 생명 중요 |
| 드론 제어 | ~2000 | 3 | 높음 | 실시간 제어 |
| 고급 매크로 | ~300 | 3 | 최소 | 매크로 학습 |

---

## 🎯 학습 목표별 예제 선택

### 🔰 처음 시작하는 경우
1. **01_beginner_tutorial.cpp** - 기본 개념 학습
2. **06_advanced_macros_example.cpp** - 고급 매크로 학습
3. **02_smart_factory_example.cpp** - 실전 응용 이해

### 🏭 산업 자동화 관심
- **02_smart_factory_example.cpp** - 생산 라인 제어
- **04_medical_device_example.cpp** - 정밀 모니터링

### 🚗 자율주행/로보틱스 관심
- **03_autonomous_vehicle_example.cpp** - 센서 융합
- **05_drone_control_example.cpp** - 실시간 제어

### 🏥 의료/안전 시스템 관심
- **04_medical_device_example.cpp** - 생명 중요 시스템
- **03_autonomous_vehicle_example.cpp** - 안전 중요 시스템

---

## 💡 주요 학습 포인트

### Actor Model 핵심 개념
- **완전한 캡슐화**: 각 Agent는 독립적
- **메시지 기반 통신**: 공유 상태 없음
- **비동기 처리**: 높은 성능과 안정성
- **장애 격리**: 한 Agent 실패가 전체에 영향 없음

### Mini SObjectizer 특징
- **제로 오버헤드**: C++17 최적화
- **실시간 안전**: 예측 가능한 성능
- **임베디드 최적화**: 최소 메모리 사용
- **사용자 친화적**: 간편한 매크로 API

### 실전 설계 패턴
- **센서 Agent 패턴**: 독립적 데이터 수집
- **제어 Agent 패턴**: 중앙 집중식 판단
- **모니터링 Agent 패턴**: 시스템 감시
- **융합 Agent 패턴**: 다중 데이터 통합

---

## 🔗 관련 문서

- [API Reference](../docs/API_REFERENCE.md) - 전체 API 문서
- [Architecture Guide](../docs/ARCHITECTURE.md) - 아키텍처 설명
- [Getting Started](../docs/GETTING_STARTED.md) - 시작 가이드
- [CLAUDE.md](../CLAUDE.md) - 개발 지침

---

## 🤝 기여하기

새로운 예제를 추가하고 싶다면:
1. 기존 예제 구조를 따라 작성
2. User-friendly 매크로 사용
3. 충분한 주석과 설명 포함
4. 실전 응용 사례 반영

**예제 명명 규칙:** `NN_domain_description_example.cpp`

---

## ⚡ 다음 단계

1. **자신의 도메인에 적용**: 기존 예제를 참고해 새로운 응용 개발
2. **성능 최적화**: 더 복잡한 시나리오로 확장
3. **실제 하드웨어 연동**: PlatformIO로 임베디드 시스템에 배포
4. **커뮤니티 기여**: 새로운 예제나 개선사항 제안

Happy coding with Mini SObjectizer! 🚀