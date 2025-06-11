# Archive Headers

이 폴더에는 Mini SObjectizer 개발 과정에서 사용된 이전 버전의 헤더 파일들이 보관되어 있습니다.

## 포함된 파일들

### Phase 1 & Phase 2 헤더들
- `mini_so_*.h` - 모듈별 헤더 파일들
- `mini_sobjectizer.h` - 이전 버전의 메인 헤더

### 안전성 패치 관련
- `message_safety_patch.h` - 메모리 안전성 패치
- `safe_message_*.h` - 안전한 메시지 설계 헤더들

## 참고사항

- 이 파일들은 **호환성 유지** 목적으로 보관됩니다
- **Production 환경**에서는 `mini_sobjectizer.h`만 사용하세요
- 개발 히스토리 참조나 이전 버전 호환성이 필요한 경우에만 사용

## 현재 권장사항

```cpp
// ✅ 권장: Production-ready 헤더
#include "mini_sobjectizer/mini_sobjectizer.h"

// ❌ 비권장: Archive 헤더들 (호환성 목적으로만)
#include "mini_sobjectizer/archive/mini_sobjectizer.h"
```