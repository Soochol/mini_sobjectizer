# FreeRTOS Heap Implementation Selection

Mini SObjectizer는 다양한 FreeRTOS 힙 구현을 제공합니다.

## 사용 가능한 힙 구현

### heap_1.c ⚡ (단순)
```c
// 특징: 메모리 해제 불가, 매우 단순
// 용도: 정적 할당 환경, 극도로 단순한 시스템
#define configSUPPORT_DYNAMIC_ALLOCATION  0
#define configSUPPORT_STATIC_ALLOCATION   1
```

### heap_2.c 🔄 (빠른 할당/해제)
```c
// 특징: 빠른 할당/해제, 고정 크기 블록
// 용도: 동일한 크기 블록을 자주 할당/해제
// 단점: 메모리 단편화 가능
```

### heap_3.c 🔗 (표준 malloc/free)
```c
// 특징: 시스템 malloc/free 사용
// 용도: 기존 C 라이브러리와 호환 필요시
#define configUSE_NEWLIB_REENTRANT        1
```

### **heap_4.c ⭐ (기본값, 권장)**
```c
// 특징: 최적 성능, 메모리 결합, 최적화됨
// 용도: 대부분의 애플리케이션 (현재 기본값)
// 장점: 단편화 방지, 인접 블록 결합
```

### heap_5.c 🏗️ (다중 힙 영역)
```c
// 특징: 여러 메모리 영역 지원
// 용도: 복잡한 메모리 레이아웃 (외부 RAM 등)
extern void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions );
```

## 힙 변경 방법

### 1. 파일 교체
```bash
# 현재 heap_4.c 제거
rm lib/freertos_minimal/heap_4.c

# 원하는 힙으로 교체 (예: heap_1.c)
cp lib/freertos_minimal/heap_1.c lib/freertos_minimal/heap_current.c
```

### 2. CMakeLists.txt 수정 (선택적)
```cmake
# 특정 힙 강제 지정
set(FREERTOS_HEAP_IMPL "heap_1")
target_sources(mini_sobjectizer PRIVATE
    lib/freertos_minimal/${FREERTOS_HEAP_IMPL}.c
)
```

### 3. FreeRTOSConfig.h 조정
```c
// heap_1 사용시
#define configSUPPORT_DYNAMIC_ALLOCATION  0
#define configSUPPORT_STATIC_ALLOCATION   1

// heap_3 사용시  
#define configUSE_NEWLIB_REENTRANT        1

// heap_5 사용시
#define configAPPLICATION_ALLOCATED_HEAP  1
```

## 권장사항

| 사용 케이스 | 권장 힙 | 이유 |
|-------------|---------|------|
| **일반적인 임베디드** | heap_4 | 최적 성능, 단편화 방지 |
| **극도로 단순한 시스템** | heap_1 | 오버헤드 최소 |
| **기존 C 코드 호환** | heap_3 | 표준 malloc/free |
| **복잡한 메모리 구조** | heap_5 | 다중 영역 지원 |

**기본값**: `heap_4.c`가 대부분의 경우에 최적입니다.