# FreeRTOS Portable Layer Directory

이 디렉토리는 Mini SObjectizer에서 지원하는 FreeRTOS 포터블 레이어를 포함합니다.

## 지원 아키텍처

### ✅ ARM_CM3 (기본 지원)
- **타겟**: STM32F103RC (Cortex-M3)
- **상태**: 완전 테스트됨
- **파일**: `ARM_CM3/port.c`, `ARM_CM3/portmacro.h`

### ✅ ARM_CM4F (추가 지원)
- **타겟**: STM32F4xx (Cortex-M4F with FPU)
- **상태**: 공식 FreeRTOS 포트
- **파일**: `ARM_CM4F/port.c`, `ARM_CM4F/portmacro.h`
- **특징**: 하드웨어 부동소수점 지원

## 사용 방법

### 아키텍처 선택
```cmake
# CMakeLists.txt에서 플랫폼 설정
if(DEFINED STM32F4)
    set(FREERTOS_PORT "ARM_CM4F")
elseif(DEFINED STM32F103)
    set(FREERTOS_PORT "ARM_CM3")  # 기본값
endif()
```

### FreeRTOSConfig.h 수정
```c
// STM32F4xx 예시
#define configCPU_CLOCK_HZ              (168000000)
#define configTOTAL_HEAP_SIZE           ((size_t)(64 * 1024))

// STM32F103 예시 (기본값)
#define configCPU_CLOCK_HZ              (72000000)
#define configTOTAL_HEAP_SIZE           ((size_t)(16 * 1024))
```

## 다른 아키텍처 추가

다른 ARM 코어나 아키텍처를 사용하려면:

1. **해당 포트 디렉토리 생성**
```bash
mkdir portable/ARM_CM7
```

2. **FreeRTOS 공식 포트 파일 복사**
- `port.c` - 저수준 커널 구현
- `portmacro.h` - 포트별 매크로 정의

3. **CMakeLists.txt 업데이트**
```cmake
target_sources(mini_sobjectizer PRIVATE
    lib/freertos_minimal/portable/${FREERTOS_PORT}/port.c
)
```

자세한 포팅 가이드는 `docs/PORTING_GUIDE.md`를 참조하세요.