# FreeRTOS Porting Guide for Mini SObjectizer

이 가이드는 Mini SObjectizer를 다른 플랫폼이나 FreeRTOS 설정으로 포팅하는 방법을 설명합니다.

## 다른 ARM 아키텍처 사용

### ARM Cortex-M4/M4F로 변경

```bash
# 1. 새로운 포트 디렉토리 생성
mkdir -p lib/freertos_minimal/portable/ARM_CM4F

# 2. FreeRTOS 공식 포트 파일 복사
# 다음 파일들을 가져와야 함:
# - portable/GCC/ARM_CM4F/port.c
# - portable/GCC/ARM_CM4F/portmacro.h
```

**FreeRTOSConfig.h 수정:**
```c
// STM32F4xx (Cortex-M4F) 설정 예시
#define configCPU_CLOCK_HZ                (168000000)  // 168MHz
#define configTOTAL_HEAP_SIZE             ((size_t)(64 * 1024))  // 64KB
#define configPRIO_BITS                   4  // CM4F도 4비트
```

### ARM Cortex-M7로 변경

```bash
# 새로운 포트 디렉토리 생성
mkdir -p lib/freertos_minimal/portable/ARM_CM7
```

**중요 고려사항:**
- **r0p1 버전**: 특별한 포트 사용 필요 (`ARM_CM7/r0p1`)
- **MPU 지원**: 16개 리전 지원 (CM3/CM4는 8개)
- **캐시 관리**: L1 캐시 일관성 고려 필요

**FreeRTOSConfig.h 수정:**
```c
// STM32H7xx (Cortex-M7) 설정 예시
#define configCPU_CLOCK_HZ                (400000000)  // 400MHz
#define configTOTAL_HEAP_SIZE             ((size_t)(128 * 1024))  // 128KB
#define configPRIO_BITS                   4
// CM7 r0p1 체크
#ifdef STM32H7xx_r0p1
  #define configUSE_CM7_R0P1_WORKAROUND   1
#endif
```

## 다른 힙 알고리즘 사용

현재: `heap_4.c` (최적의 성능, 메모리 결합)

### heap_1.c로 변경 (단순, 해제 불가)
```c
// 매우 단순한 임베디드 시스템용
// 메모리 해제 불가 - 정적 할당만
#define configSUPPORT_DYNAMIC_ALLOCATION  0
#define configSUPPORT_STATIC_ALLOCATION   1
```

### heap_2.c로 변경 (빠른 할당/해제)
```c
// 고정 크기 블록 사용
// 단편화 가능성 있음
```

### heap_3.c로 변경 (표준 malloc/free)
```c
// 시스템의 malloc/free 사용
// 쓰레드 안전성 래퍼 제공
#define configUSE_NEWLIB_REENTRANT        1
```

### heap_5.c로 변경 (다중 힙 영역)
```c
// 여러 메모리 영역 사용 가능
// 복잡한 메모리 레이아웃용
extern void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions );
```

**변경 방법:**
```bash
# 1. 원하는 힙 파일로 교체
rm lib/freertos_minimal/heap_4.c
cp path/to/freertos/portable/MemMang/heap_5.c lib/freertos_minimal/

# 2. CMakeLists.txt 업데이트 (자동으로 glob됨)
```

## ESP32로 포팅

ESP32는 자체 FreeRTOS 통합 제공:

```cpp
// platformio.ini
[env:esp32]
platform = espressif32
framework = arduino  // 또는 espidf
board = esp32dev

// ESP32 전용 설정
#define configTICK_RATE_HZ                1000  // 1ms tick
#define configNUM_CORES                   2     // 듀얼코어
#define configUSE_CORE_AFFINITY           1     // 코어 선호도
```

**주의사항:**
- ESP32는 자체 FreeRTOS 구현 사용
- `lib/freertos_minimal` 제거하고 ESP-IDF 사용
- Xtensa/RISC-V 아키텍처별 최적화

## AVR (Arduino) 포팅

```cpp
// Arduino용 FreeRTOS 라이브러리 설치 필요
// "FreeRTOS" in Arduino Library Manager

// 설정 차이점
#define configTICK_RATE_HZ                62    // 62Hz (not 1000Hz)
#define configTOTAL_HEAP_SIZE             ((size_t)(1024))  // 1KB
#define configMINIMAL_STACK_SIZE          85    // 더 작은 스택
```

## 플랫폼별 CMake 설정

```cmake
# CMakeLists.txt에 플랫폼 감지 추가
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    if(DEFINED STM32F4)
        set(FREERTOS_PORT "ARM_CM4F")
    elseif(DEFINED STM32H7)
        set(FREERTOS_PORT "ARM_CM7")
    else()
        set(FREERTOS_PORT "ARM_CM3")  # 기본값
    endif()
    
    # 포트별 소스 파일 추가
    target_sources(mini_sobjectizer PRIVATE
        lib/freertos_minimal/portable/${FREERTOS_PORT}/port.c
    )
    
    target_include_directories(mini_sobjectizer PRIVATE
        lib/freertos_minimal/portable/${FREERTOS_PORT}
    )
endif()
```

## 타이밍 설정 조정

```c
// 플랫폼별 타이밍 최적화
#ifdef STM32F103  // 72MHz
    #define configCPU_CLOCK_HZ            72000000
    #define configTICK_RATE_HZ            1000
#elif STM32F4     // 168MHz
    #define configCPU_CLOCK_HZ            168000000
    #define configTICK_RATE_HZ            1000
#elif STM32H7     // 400MHz
    #define configCPU_CLOCK_HZ            400000000
    #define configTICK_RATE_HZ            1000
#elif ESP32       // 240MHz dual-core
    #define configCPU_CLOCK_HZ            240000000
    #define configTICK_RATE_HZ            1000
#endif
```

## 검증 방법

포팅 후 반드시 테스트:

```bash
# 1. 기본 기능 테스트
cd build && ctest -R test_core_functionality

# 2. 플랫폼별 성능 테스트
cd build && ctest -R test_lockfree_performance

# 3. 메모리 안전성 테스트
cd build && ctest -R test_memory_safety
```

## 문제 해결

### ARM CM7 r0p1 이슈
```c
// 특정 리비전에서 명령어 호환성 문제
#if defined(STM32H7xx) && defined(CORE_CM7_R0P1)
    #error "Use ARM_CM7/r0p1 port for this revision"
#endif
```

### MPU 설정 불일치
```c
// CM7에서 MPU 리전 수 확인
#if defined(__ARM_ARCH_7EM__) && defined(__MPU_PRESENT)
    #define portEXPECTED_MPU_TYPE_VALUE    (16UL << 8UL)  // CM7: 16 regions
#else
    #define portEXPECTED_MPU_TYPE_VALUE    (8UL << 8UL)   // CM3/CM4: 8 regions
#endif
```

이 가이드를 통해 Mini SObjectizer를 다양한 플랫폼으로 안전하게 포팅할 수 있습니다.