#pragma once

#include "mini_sobjectizer.h"
#include <new>

namespace mini_so {
namespace safety {

// ============================================================================
// 즉시 적용 가능한 안전성 패치
// ============================================================================

// 1. 컴파일 타임 타입 안전성 검증
template<typename T>
constexpr bool is_memcpy_safe_message() noexcept {
    using MessageType = Message<T>;
    
    // 기본 안전성 요구사항
    return std::is_trivially_copyable_v<MessageType> &&
           std::is_trivially_destructible_v<MessageType> &&
           std::is_standard_layout_v<MessageType> &&
           !std::is_polymorphic_v<MessageType> &&
           sizeof(MessageType) <= MINI_SO_MAX_MESSAGE_SIZE;
}

// 2. 런타임 메시지 무결성 검증
struct MessageIntegrityCheck {
    uint32_t magic_number;      // 0xDEADBEEF
    uint32_t type_id_check;     // type_id의 복사본
    uint32_t size_check;        // 메시지 크기
    uint32_t checksum;          // 간단한 체크섬
    
    static constexpr uint32_t MAGIC = 0xDEADBEEF;
    
    template<typename T>
    static MessageIntegrityCheck create(const Message<T>& msg) noexcept {
        MessageIntegrityCheck check;
        check.magic_number = MAGIC;
        check.type_id_check = MESSAGE_TYPE_ID(T);
        check.size_check = sizeof(Message<T>);
        
        // 간단한 체크섬 계산
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&msg);
        uint32_t sum = 0;
        for (size_t i = 0; i < sizeof(Message<T>); ++i) {
            sum += data[i];
        }
        check.checksum = sum ^ check.type_id_check ^ check.size_check;
        
        return check;
    }
    
    template<typename T>
    bool verify(const Message<T>& msg) const noexcept {
        if (magic_number != MAGIC) return false;
        if (type_id_check != MESSAGE_TYPE_ID(T)) return false;
        if (size_check != sizeof(Message<T>)) return false;
        
        // 체크섬 검증
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&msg);
        uint32_t sum = 0;
        for (size_t i = 0; i < sizeof(Message<T>); ++i) {
            sum += data[i];
        }
        uint32_t expected_checksum = sum ^ type_id_check ^ size_check;
        
        return checksum == expected_checksum;
    }
};

// 3. 안전한 메시지 엔트리 (기존 QueueEntry 대체)
struct alignas(64) SafeQueueEntry {
    alignas(8) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE - sizeof(MessageIntegrityCheck)];
    MessageIntegrityCheck integrity;
    uint16_t size;
    MessageId type_id;      // 추가 검증용
    bool valid;
    uint8_t padding[39];    // 64바이트 정렬
};

// 4. 안전한 메시지 복사 함수들
template<typename T>
bool safe_message_copy_in(SafeQueueEntry& entry, const Message<T>& msg) noexcept {
    // 컴파일 타임 안전성 검증
    static_assert(is_memcpy_safe_message<T>(), 
                  "Message type is not safe for memcpy operations");
    
    constexpr uint16_t msg_size = sizeof(Message<T>);
    static_assert(msg_size <= sizeof(entry.data), "Message too large for safe storage");
    
    // 무결성 체크 생성
    entry.integrity = MessageIntegrityCheck::create(msg);
    entry.size = msg_size;
    entry.type_id = MESSAGE_TYPE_ID(T);
    entry.valid = true;
    
    // 안전한 복사 (POD 타입 보장됨)
    std::memcpy(entry.data, &msg, msg_size);
    
    return true;
}

template<typename T>
bool safe_message_copy_out(const SafeQueueEntry& entry, Message<T>& msg) noexcept {
    // 기본 검증
    if (!entry.valid) return false;
    if (entry.type_id != MESSAGE_TYPE_ID(T)) return false;
    if (entry.size != sizeof(Message<T>)) return false;
    
    // 메시지 복사
    std::memcpy(&msg, entry.data, entry.size);
    
    // 무결성 검증
    if (!entry.integrity.verify(msg)) {
        // 무결성 오류 - 메시지 손상됨
        System::instance().report_error(
            system_messages::ErrorReport::CRITICAL,
            2001, // MESSAGE_INTEGRITY_ERROR
            INVALID_AGENT_ID
        );
        return false;
    }
    
    return true;
}

// 5. 기존 MessageQueue의 안전한 대체품
class SafeMessageQueuePatch {
private:
    alignas(64) std::array<SafeQueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    alignas(64) std::size_t head_{0};
    alignas(64) std::size_t tail_{0};
    alignas(64) std::size_t count_{0};
    SemaphoreHandle_t mutex_;
    
    // 통계 (디버깅용)
    uint32_t integrity_failures_{0};
    uint32_t type_mismatches_{0};

public:
    SafeMessageQueuePatch() noexcept {
        mutex_ = xSemaphoreCreateMutex();
        if (!mutex_) [[unlikely]] {
            taskDISABLE_INTERRUPTS();
            for(;;);
        }
        
        for (auto& entry : queue_) {
            entry.valid = false;
            entry.size = 0;
        }
    }
    
    ~SafeMessageQueuePatch() noexcept {
        if (mutex_) {
            vSemaphoreDelete(mutex_);
        }
    }
    
    template<typename T>
    MessageQueue::Result push(const Message<T>& msg) noexcept {
        // 컴파일 타임 안전성 검증
        if constexpr (!is_memcpy_safe_message<T>()) {
            // 컴파일 타임에 감지되므로 이 코드는 실행되지 않음
            static_assert(is_memcpy_safe_message<T>(), 
                         "Message type is not safe for storage");
            return MessageQueue::Result::INVALID_MESSAGE;
        }
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return MessageQueue::Result::INVALID_MESSAGE;
        }
        
        if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
            xSemaphoreGive(mutex_);
            return MessageQueue::Result::QUEUE_FULL;
        }
        
        SafeQueueEntry& entry = queue_[tail_];
        
        // 안전한 복사 (무결성 검사 포함)
        if (!safe_message_copy_in(entry, msg)) {
            xSemaphoreGive(mutex_);
            return MessageQueue::Result::INVALID_MESSAGE;
        }
        
        tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_++;
        
        xSemaphoreGive(mutex_);
        return MessageQueue::Result::SUCCESS;
    }
    
    template<typename T>
    bool pop(Message<T>& msg) noexcept {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        
        if (count_ == 0) {
            xSemaphoreGive(mutex_);
            return false;
        }
        
        SafeQueueEntry& entry = queue_[head_];
        
        // 타입 안전한 복사 (무결성 검사 포함)
        bool success = safe_message_copy_out(entry, msg);
        
        if (success) {
            entry.valid = false;
            entry.size = 0;
            head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
            count_--;
        } else {
            // 무결성 실패 통계 업데이트
            if (entry.type_id != MESSAGE_TYPE_ID(T)) {
                type_mismatches_++;
            } else {
                integrity_failures_++;
            }
            
            // 손상된 엔트리 제거
            entry.valid = false;
            entry.size = 0;
            head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
            count_--;
        }
        
        xSemaphoreGive(mutex_);
        return success;
    }
    
    // 기존 호환성을 위한 unsafe 메서드 (deprecated)
    [[deprecated("Use type-safe pop<T>() instead")]]
    bool pop_unsafe(uint8_t* buffer, uint16_t& size) noexcept {
        // 임시 호환성 제공 - 점진적 마이그레이션용
        if (!buffer) return false;
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        
        if (count_ == 0) {
            xSemaphoreGive(mutex_);
            return false;
        }
        
        SafeQueueEntry& entry = queue_[head_];
        if (!entry.valid) {
            xSemaphoreGive(mutex_);
            return false;
        }
        
        size = entry.size;
        std::memcpy(buffer, entry.data, entry.size);
        entry.valid = false;
        entry.size = 0;
        
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
        
        xSemaphoreGive(mutex_);
        return true;
    }
    
    bool empty() const noexcept { return count_ == 0; }
    bool full() const noexcept { return count_ >= MINI_SO_MAX_QUEUE_SIZE; }
    std::size_t size() const noexcept { return count_; }
    
    // 디버깅 정보
    uint32_t get_integrity_failures() const noexcept { return integrity_failures_; }
    uint32_t get_type_mismatches() const noexcept { return type_mismatches_; }
    
    void clear() noexcept {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            for (auto& entry : queue_) {
                entry.valid = false;
                entry.size = 0;
            }
            head_ = 0;
            tail_ = 0;
            count_ = 0;
            xSemaphoreGive(mutex_);
        }
    }
};

} // namespace safety
} // namespace mini_so