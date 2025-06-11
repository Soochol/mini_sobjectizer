#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace mini_so {

// ============================================================================
// Safe Message System - POD 기반 안전한 메시지 시스템
// ============================================================================

// 메시지 헤더 - POD 타입으로 설계
struct alignas(8) SafeMessageHeader {
    MessageId type_id;
    AgentId sender_id;
    uint32_t timestamp;
    uint16_t payload_size;
    uint16_t reserved;  // 8바이트 정렬용
    
    constexpr SafeMessageHeader(MessageId id, AgentId sender = INVALID_AGENT_ID) noexcept
        : type_id(id), sender_id(sender), timestamp(0), payload_size(0), reserved(0) {}
};

// POD 메시지 베이스 - virtual 함수 제거
class SafeMessageBase {
public:
    SafeMessageHeader header;
    
    constexpr SafeMessageBase(MessageId id, AgentId sender = INVALID_AGENT_ID) noexcept
        : header(id, sender) {}
    
    // virtual 제거 - POD 보장
    // virtual ~SafeMessageBase() = default;  // 제거!
    
    // 접근자들 - 모두 constexpr
    constexpr MessageId type_id() const noexcept { return header.type_id; }
    constexpr AgentId sender_id() const noexcept { return header.sender_id; }
    constexpr TimePoint timestamp() const noexcept { return header.timestamp; }
    constexpr uint16_t payload_size() const noexcept { return header.payload_size; }
    
    void mark_sent() noexcept { 
        header.timestamp = now(); 
    }
};

// 컴파일 타임 타입 안전성 검사
template<typename T>
struct is_safe_message : std::false_type {};

template<typename T>
constexpr bool is_safe_message_v = is_safe_message<T>::value;

// 안전한 메시지 템플릿
template<typename T>
class SafeMessage {
    // 컴파일 타임 안전성 검증
    static_assert(std::is_trivially_copyable_v<T>, 
                  "Message payload must be trivially copyable");
    static_assert(std::is_standard_layout_v<T>, 
                  "Message payload must have standard layout");
    static_assert(!std::is_polymorphic_v<T>, 
                  "Message payload cannot have virtual functions");
    static_assert(sizeof(T) <= MINI_SO_MAX_MESSAGE_SIZE - sizeof(SafeMessageHeader),
                  "Message payload too large");

public:
    SafeMessageHeader header;
    T data;
    
    constexpr SafeMessage(const T& msg_data, AgentId sender = INVALID_AGENT_ID) noexcept
        : header(MESSAGE_TYPE_ID(T), sender), data(msg_data) {
        header.payload_size = sizeof(T);
    }
    
    template<typename... Args>
    constexpr SafeMessage(AgentId sender, Args&&... args) noexcept
        : header(MESSAGE_TYPE_ID(T), sender), data(std::forward<Args>(args)...) {
        header.payload_size = sizeof(T);
    }
    
    // 타입 안전한 접근자
    constexpr MessageId type_id() const noexcept { return header.type_id; }
    constexpr AgentId sender_id() const noexcept { return header.sender_id; }
    constexpr TimePoint timestamp() const noexcept { return header.timestamp; }
    
    void mark_sent() noexcept { header.timestamp = now(); }
    
    // memcpy 안전성 보장
    constexpr bool is_memcpy_safe() const noexcept {
        return std::is_trivially_copyable_v<SafeMessage<T>>;
    }
};

// 특화: SafeMessage는 안전한 메시지임을 표시
template<typename T>
struct is_safe_message<SafeMessage<T>> : std::true_type {};

// ============================================================================
// Safe Message Queue - 타입 안전성 보장
// ============================================================================
class SafeMessageQueue {
public:
    enum class Result : uint8_t {
        SUCCESS = 0,
        QUEUE_FULL = 1,
        MESSAGE_TOO_LARGE = 2,
        INVALID_MESSAGE = 3,
        TYPE_UNSAFE = 4
    };

private:
    struct alignas(64) QueueEntry {
        alignas(8) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];
        uint16_t size;
        bool valid;
        uint8_t padding[45];
    };
    
    alignas(64) std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    alignas(64) std::size_t head_{0};
    alignas(64) std::size_t tail_{0};
    alignas(64) std::size_t count_{0};
    SemaphoreHandle_t mutex_;

public:
    SafeMessageQueue() noexcept;
    ~SafeMessageQueue() noexcept;
    
    // 타입 안전한 push - 컴파일 타임 검증
    template<typename T>
    Result push(const SafeMessage<T>& msg) noexcept {
        // 컴파일 타임 안전성 검증
        static_assert(is_safe_message_v<SafeMessage<T>>, 
                      "Only SafeMessage types are allowed");
        static_assert(std::is_trivially_copyable_v<SafeMessage<T>>, 
                      "Message must be trivially copyable for safe memcpy");
        
        constexpr uint16_t msg_size = sizeof(SafeMessage<T>);
        static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, "Message too large");
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return Result::INVALID_MESSAGE;
        }
        
        if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
            xSemaphoreGive(mutex_);
            return Result::QUEUE_FULL;
        }
        
        QueueEntry& entry = queue_[tail_];
        
        // 이제 안전한 memcpy (POD 타입 보장됨)
        std::memcpy(entry.data, &msg, msg_size);
        entry.size = msg_size;
        entry.valid = true;
        
        tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_++;
        
        xSemaphoreGive(mutex_);
        return Result::SUCCESS;
    }
    
    // 타입 안전한 pop
    template<typename T>
    bool pop(SafeMessage<T>& msg) noexcept {
        static_assert(is_safe_message_v<SafeMessage<T>>, 
                      "Only SafeMessage types are allowed");
        static_assert(std::is_trivially_copyable_v<SafeMessage<T>>, 
                      "Message must be trivially copyable for safe memcpy");
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        
        if (count_ == 0) {
            xSemaphoreGive(mutex_);
            return false;
        }
        
        QueueEntry& entry = queue_[head_];
        if (!entry.valid) {
            xSemaphoreGive(mutex_);
            return false;
        }
        
        constexpr uint16_t expected_size = sizeof(SafeMessage<T>);
        if (entry.size != expected_size) {
            // 타입 불일치 - 다른 타입의 메시지
            xSemaphoreGive(mutex_);
            return false;
        }
        
        // 안전한 memcpy (POD 타입 보장됨)
        std::memcpy(&msg, entry.data, entry.size);
        entry.valid = false;
        entry.size = 0;
        
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
        
        xSemaphoreGive(mutex_);
        return true;
    }
    
    // 범용 타입 확인 후 pop (기존 코드 호환성용)
    bool pop_unsafe(uint8_t* buffer, uint16_t& size) noexcept {
        // 이 함수는 deprecated로 표시하고 점진적으로 제거
        [[deprecated("Use type-safe pop<T>() instead")]]
        if (!buffer) return false;
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        
        if (count_ == 0) {
            xSemaphoreGive(mutex_);
            return false;
        }
        
        QueueEntry& entry = queue_[head_];
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
    void clear() noexcept;
};

} // namespace mini_so