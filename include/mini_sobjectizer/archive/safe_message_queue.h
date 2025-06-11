#pragma once

#include "mini_sobjectizer.h"
#include <memory>

namespace mini_so {

// 안전한 메시지 큐 - 포인터 기반 접근법
class SafeMessageQueue {
public:
    enum class Result : uint8_t {
        SUCCESS = 0,
        QUEUE_FULL = 1,
        INVALID_MESSAGE = 2,
        MEMORY_ERROR = 3
    };

private:
    // 메시지 엔트리 - 포인터와 메타데이터만 저장
    struct alignas(64) QueueEntry {
        MessageBase* message_ptr;       // 8 bytes
        MessageId type_id;              // 2 bytes  
        uint16_t message_size;          // 2 bytes
        AgentId sender_id;              // 2 bytes
        TimePoint timestamp;            // 4 bytes
        bool valid;                     // 1 byte
        uint8_t padding[45];            // 64 byte alignment
    };
    
    alignas(64) std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    alignas(64) std::size_t head_{0};
    alignas(64) std::size_t tail_{0};
    alignas(64) std::size_t count_{0};
    SemaphoreHandle_t mutex_;

public:
    SafeMessageQueue() noexcept;
    ~SafeMessageQueue() noexcept;
    
    // 메시지 포인터 저장 (소유권 이전)
    template<typename T>
    Result push_owned(std::unique_ptr<Message<T>> msg) noexcept {
        if (!msg) return Result::INVALID_MESSAGE;
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return Result::INVALID_MESSAGE;
        }
        
        if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
            xSemaphoreGive(mutex_);
            return Result::QUEUE_FULL;
        }
        
        QueueEntry& entry = queue_[tail_];
        entry.message_ptr = msg.release();  // 소유권 이전
        entry.type_id = MESSAGE_TYPE_ID(T);
        entry.message_size = sizeof(Message<T>);
        entry.sender_id = entry.message_ptr->sender_id();
        entry.timestamp = now();
        entry.valid = true;
        
        tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_++;
        
        xSemaphoreGive(mutex_);
        return Result::SUCCESS;
    }
    
    // 메시지 풀에서 할당된 메시지 저장
    template<typename T>
    Result push_pooled(PooledMessage<T>&& pooled_msg) noexcept {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return Result::INVALID_MESSAGE;
        }
        
        if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
            xSemaphoreGive(mutex_);
            return Result::QUEUE_FULL;
        }
        
        QueueEntry& entry = queue_[tail_];
        
        // 풀링된 메시지의 포인터를 저장 (소유권 이전하지 않음)
        entry.message_ptr = const_cast<Message<T>*>(&pooled_msg.get());
        entry.type_id = MESSAGE_TYPE_ID(T);
        entry.message_size = sizeof(Message<T>);
        entry.sender_id = entry.message_ptr->sender_id();
        entry.timestamp = now();
        entry.valid = true;
        
        tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_++;
        
        xSemaphoreGive(mutex_);
        return Result::SUCCESS;
    }
    
    // 타입 안전한 메시지 팝
    std::unique_ptr<MessageBase> pop() noexcept {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return nullptr;
        }
        
        if (count_ == 0) {
            xSemaphoreGive(mutex_);
            return nullptr;
        }
        
        QueueEntry& entry = queue_[head_];
        if (!entry.valid || !entry.message_ptr) {
            xSemaphoreGive(mutex_);
            return nullptr;
        }
        
        // 소유권을 unique_ptr로 래핑
        std::unique_ptr<MessageBase> result(entry.message_ptr);
        
        // 엔트리 정리
        entry.message_ptr = nullptr;
        entry.valid = false;
        
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
        
        xSemaphoreGive(mutex_);
        return result;
    }
    
    bool empty() const noexcept { return count_ == 0; }
    bool full() const noexcept { return count_ >= MINI_SO_MAX_QUEUE_SIZE; }
    std::size_t size() const noexcept { return count_; }
    
    void clear() noexcept {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            // 모든 메시지 정리
            for (std::size_t i = 0; i < MINI_SO_MAX_QUEUE_SIZE; ++i) {
                if (queue_[i].valid && queue_[i].message_ptr) {
                    delete queue_[i].message_ptr;
                    queue_[i].message_ptr = nullptr;
                }
                queue_[i].valid = false;
            }
            head_ = 0;
            tail_ = 0;
            count_ = 0;
            xSemaphoreGive(mutex_);
        }
    }
};

} // namespace mini_so