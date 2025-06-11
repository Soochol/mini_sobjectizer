# Development Tests

이 폴더에는 개발 과정에서 사용된 디버깅, 분석, 성능 평가 테스트들이 포함되어 있습니다.

## 테스트 분류

### 디버깅 테스트
- `test_step_by_step.cpp` - 단계별 디버깅 테스트
- `test_debug_segfault.cpp` - Segmentation fault 디버깅
- `test_basic_types.cpp` - 기본 타입 동작 테스트
- `test_minimal_functionality.cpp` - 최소 기능 테스트

### 안전성 검증 테스트  
- `test_simple_safety.cpp` - 기본 안전성 검증
- `test_core_safe.cpp` - 핵심 안전성 테스트

### 분석 및 평가 테스트
- `test_watchdog_precision_analysis.cpp` - Watchdog 정밀도 분석
- `test_error_log_overflow.cpp` - 에러 로그 오버플로우 분석
- `test_lockfree_performance.cpp` - Lock-free 성능 측정

## 용도

이 테스트들은 다음 용도로 사용됩니다:

1. **문제 진단**: 시스템 이슈 디버깅
2. **성능 분석**: 각 컴포넌트별 성능 측정
3. **요구사항 분석**: 기능 개선 필요성 평가
4. **단계별 검증**: 개발 과정의 중간 검증

## 참고사항

이 테스트들은 개발/디버깅 목적이므로 production 환경에서는 실행할 필요가 없습니다.
Production 환경에서는 `../production_ready_tests/` 폴더의 테스트들을 사용하세요.