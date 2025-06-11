// 기존 MessageQueue::push 메서드의 안전한 패치
#include "mini_sobjectizer/mini_sobjectizer.h"
#include <type_traits>

namespace mini_so {

// 기존 MessageQueue::push를 안전한 버전으로 교체
MessageQueue::Result MessageQueue::push(const MessageBase& msg, uint16_t size) noexcept {
    if (size > MINI_SO_MAX_MESSAGE_SIZE) [[unlikely]] {
        return Result::MESSAGE_TOO_LARGE;
    }
    
    // 🔥 핵심 안전성 검사 추가
    // MessageBase의 virtual 함수가 있는지 확인
    if (std::is_polymorphic_v<decltype(msg)>) [[unlikely]] {
        // Virtual 함수가 있는 객체는 memcpy 불안전
        System::instance().report_error(
            system_messages::ErrorReport::CRITICAL,
            3001, // UNSAFE_MESSAGE_TYPE
            INVALID_AGENT_ID
        );
        return Result::INVALID_MESSAGE;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) [[unlikely]] {
        return Result::INVALID_MESSAGE;
    }
    
    if (count_ >= MINI_SO_MAX_QUEUE_SIZE) [[unlikely]] {
        xSemaphoreGive(mutex_);
        return Result::QUEUE_FULL;
    }
    
    QueueEntry& entry = queue_[tail_];
    
    // 🔥 안전성 향상된 복사
    if (size <= MINI_SO_MAX_MESSAGE_SIZE) [[likely]] {
        // 추가 무결성 검사
        const uint8_t* msg_ptr = reinterpret_cast<const uint8_t*>(&msg);
        
        // 메시지 헤더 검증
        if (size >= sizeof(MessageHeader)) {
            const MessageHeader* header = reinterpret_cast<const MessageHeader*>(msg_ptr);
            
            // 기본 유효성 검증
            if (header->type_id == INVALID_MESSAGE_ID) [[unlikely]] {
                xSemaphoreGive(mutex_);
                return Result::INVALID_MESSAGE;
            }
            
            // 타임스탬프 검증 (미래 시간 체크)
            TimePoint current_time = now();
            if (header->timestamp > current_time + 1000) [[unlikely]] { // 1초 오차 허용
                xSemaphoreGive(mutex_);
                return Result::INVALID_MESSAGE;
            }
        }
        
        // 안전한 메모리 복사 (여전히 memcpy이지만 검증 후)
        std::memcpy(entry.data, &msg, size);
        entry.size = size;
        entry.valid = true;
        
        tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_++;
    } else {
        xSemaphoreGive(mutex_);
        return Result::MESSAGE_TOO_LARGE;
    }
    
    xSemaphoreGive(mutex_);
    return Result::SUCCESS;
}

// 안전한 pop 메서드 추가
bool MessageQueue::pop_safe(uint8_t* buffer, uint16_t& size) noexcept {
    if (!buffer) [[unlikely]] {
        return false;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) [[unlikely]] {
        return false;
    }
    
    if (count_ == 0) [[unlikely]] {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    QueueEntry& entry = queue_[head_];
    if (!entry.valid) [[unlikely]] {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    // 🔥 복사 전 무결성 검증
    if (entry.size <= MINI_SO_MAX_MESSAGE_SIZE && entry.size >= sizeof(MessageHeader)) [[likely]] {
        // 메시지 헤더 검증
        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(entry.data);
        
        // 기본 검증
        if (header->type_id == INVALID_MESSAGE_ID) [[unlikely]] {
            // 손상된 메시지 감지
            System::instance().report_error(
                system_messages::ErrorReport::WARNING,
                3002, // CORRUPTED_MESSAGE
                INVALID_AGENT_ID
            );
            
            // 손상된 엔트리 제거
            entry.valid = false;
            entry.size = 0;
            head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
            count_--;
            xSemaphoreGive(mutex_);
            return false;
        }
        
        size = entry.size;
        std::memcpy(buffer, entry.data, entry.size);
        entry.valid = false;
        entry.size = 0;
        
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
    } else {
        // 크기 오류 - 손상된 엔트리
        entry.valid = false;
        entry.size = 0;
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
        xSemaphoreGive(mutex_);
        return false;
    }
    
    xSemaphoreGive(mutex_);
    return true;
}

} // namespace mini_so