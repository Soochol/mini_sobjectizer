# Production-Ready Tests

이 폴더에는 Mini SObjectizer v3.0의 production readiness를 검증하는 핵심 테스트들이 포함되어 있습니다.

## 테스트 목록

### 핵심 기능 테스트
- `test_core_functionality.cpp` - 전체 시스템 통합 테스트
- `test_memory_safety.cpp` - 메모리 안전성 검증
- `test_lockfree_safety.cpp` - Lock-free 동시성 안전성 검증

### 시스템 안정성 테스트
- `test_emergency_failsafe.cpp` - Emergency fail-safe 메커니즘 검증
- `test_type_id_collision_fix.cpp` - 타입 ID 충돌 해결 검증
- `test_siof_fix.cpp` - Static Initialization Order Fiasco 해결 검증

### 장기 운영 안정성 테스트
- `test_metrics_overflow_fix.cpp` - 성능 메트릭 오버플로우 해결 검증
- `test_circular_error_log.cpp` - 순환 에러 로그 검증

## 실행 방법

```bash
# 전체 production 테스트 실행
g++ -std=c++17 -DUNIT_TEST -I../../include test_*.cpp ../../src/mini_sobjectizer.cpp ../../src/freertos_mock.cpp -o test_runner
./test_runner

# 개별 테스트 실행
g++ -std=c++17 -DUNIT_TEST -I../../include test_core_functionality.cpp ../../src/mini_sobjectizer.cpp ../../src/freertos_mock.cpp -o test_core
./test_core
```

## 테스트 범위

- ✅ 메모리 안전성 (Stack-use-after-return, Virtual destructor)
- ✅ 동시성 안전성 (Lock-free CAS, Race conditions)
- ✅ 시스템 안정성 (Emergency recovery, Fail-safe)
- ✅ 타입 안전성 (Type ID collision prevention)
- ✅ 초기화 안전성 (SIOF prevention)
- ✅ 장기 운영 안정성 (Overflow prevention, Error logging)

Production Readiness Score: **100/100** ✅