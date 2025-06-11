// Modular implementation for Mini SObjectizer
#include "mini_sobjectizer/mini_sobjectizer.h"
#include "mini_sobjectizer/mini_so_config.h"

namespace mini_so {

// ============================================================================
// MessageQueue Implementation
// ============================================================================

MessageQueue::MessageQueue() noexcept {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        // In embedded systems, this would trigger a system halt
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    // Initialize all entries
    for (auto& entry : queue_) {
        entry.valid = false;
        entry.size = 0;
    }
}

MessageQueue::~MessageQueue() noexcept {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

MessageQueue::MessageQueue(MessageQueue&& other) noexcept
    : head_(other.head_), tail_(other.tail_), count_(other.count_), mutex_(other.mutex_) {
    other.mutex_ = nullptr;
    other.head_ = 0;
    other.tail_ = 0;
    other.count_ = 0;
}

MessageQueue& MessageQueue::operator=(MessageQueue&& other) noexcept {
    if (this != &other) {
        if (mutex_) {
            vSemaphoreDelete(mutex_);
        }
        
        head_ = other.head_;
        tail_ = other.tail_;
        count_ = other.count_;
        mutex_ = other.mutex_;
        
        other.mutex_ = nullptr;
        other.head_ = 0;
        other.tail_ = 0;
        other.count_ = 0;
    }
    return *this;
}

MessageQueue::Result MessageQueue::push(const MessageBase& msg, uint16_t size) noexcept {
    if (size > MINI_SO_MAX_MESSAGE_SIZE) {
        return Result::MESSAGE_TOO_LARGE;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return Result::MUTEX_ERROR;
    }
    
    if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
        xSemaphoreGive(mutex_);
        return Result::QUEUE_FULL;
    }
    
    // Copy message to queue entry
    QueueEntry& entry = queue_[tail_];
    std::memcpy(entry.data, &msg, size);
    entry.size = size;
    entry.valid = true;
    
    tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
    count_++;
    
    xSemaphoreGive(mutex_);
    return Result::SUCCESS;
}

bool MessageQueue::pop(uint8_t* buffer, uint16_t& size) noexcept {
    if (!buffer) {
        return false;
    }
    
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
    
    // Copy message from queue entry
    size = entry.size;
    std::memcpy(buffer, entry.data, entry.size);
    entry.valid = false;
    entry.size = 0;
    
    head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
    count_--;
    
    xSemaphoreGive(mutex_);
    return true;
}

void MessageQueue::clear() noexcept {
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

// ============================================================================
// Agent Implementation
// ============================================================================

std::size_t Agent::process_messages() noexcept {
    std::size_t processed = 0;
    
    while (!message_queue_.empty() && processed < 8) {  // Prevent excessive processing
        alignas(8) uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
        uint16_t size;
        
        if (message_queue_.pop(buffer, size)) {
            const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
            if (handle_message(*msg)) {
                processed++;
            }
        } else {
            break;
        }
    }
    
    return processed;
}

bool Agent::process_one_message() noexcept {
    if (message_queue_.empty()) {
        return false;
    }
    
    alignas(8) uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    uint16_t size;
    
    if (message_queue_.pop(buffer, size)) {
        const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
        return handle_message(*msg);
    }
    
    return false;
}

Agent::MessageQueueStats Agent::get_queue_stats() const noexcept {
    MessageQueueStats stats;
    stats.current_size = message_queue_.size();
    stats.capacity = message_queue_.capacity();
    
#if MINI_SO_ENABLE_METRICS
    stats.total_processed = total_messages_processed_;
    stats.total_dropped = total_messages_dropped_;
#else
    stats.total_processed = 0;
    stats.total_dropped = 0;
#endif
    
    return stats;
}

// ============================================================================
// Framework Functions  
// ============================================================================

bool initialize() noexcept {
    // Framework initialization
    return true;
}

void shutdown() noexcept {
    // Framework shutdown
}

const char* get_info_string() noexcept {
    return "Mini SObjectizer v3.0.0 - Modular Version\n"
           "Zero-overhead Actor Model for embedded systems\n"
           "Features: Message passing, Type safety, Cache optimization";
}

// ============================================================================
// Environment Implementation
// ============================================================================

Environment* Environment::instance_ = nullptr;

Environment::Environment() noexcept {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    // Initialize agent array
    agents_.fill(nullptr);
}

Environment::~Environment() noexcept {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

bool Environment::initialize() noexcept {
    if (env_initialized_) {
        return true;
    }
    
    env_initialized_ = true;
    return true;
}

AgentId Environment::register_agent(Agent* agent) noexcept {
    if (!agent || agent_count_ >= MINI_SO_MAX_AGENTS) {
        return INVALID_AGENT_ID;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return INVALID_AGENT_ID;
    }
    
    AgentId id = static_cast<AgentId>(agent_count_);
    agents_[agent_count_] = agent;
    agent_count_++;
    
    xSemaphoreGive(mutex_);
    
    agent->initialize(id);
    return id;
}

void Environment::unregister_agent(AgentId id) noexcept {
    if (id >= agent_count_) {
        return;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (agents_[id]) {
            agents_[id]->message_queue_.clear();
            agents_[id] = nullptr;
        }
        xSemaphoreGive(mutex_);
    }
}

Agent* Environment::get_agent(AgentId id) noexcept {
    if (id >= agent_count_) {
        return nullptr;
    }
    return agents_[id];
}

bool Environment::process_one_message() noexcept {
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && !agents_[i]->message_queue_.empty()) {
            agents_[i]->process_messages();
#if MINI_SO_ENABLE_METRICS
            total_messages_processed_++;
#endif
            return true;
        }
    }
    return false;
}

void Environment::process_all_messages() noexcept {
    bool has_messages = true;
    while (has_messages) {
        has_messages = false;
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i] && !agents_[i]->message_queue_.empty()) {
                agents_[i]->process_messages();
                has_messages = true;
            }
        }
    }
}

void Environment::run() noexcept {
#if MINI_SO_ENABLE_METRICS
    TimePoint loop_start = now();
#endif
    
    process_all_messages();
    
#if MINI_SO_ENABLE_METRICS
    TimePoint loop_end = now();
    uint32_t processing_time_us = (loop_end - loop_start) * 1000;
    if (processing_time_us > max_processing_time_us_) {
        max_processing_time_us_ = processing_time_us;
    }
#endif
}

std::size_t Environment::total_pending_messages() const noexcept {
    std::size_t total = 0;
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i]) {
            total += agents_[i]->message_queue_.size();
        }
    }
    return total;
}

bool Environment::is_healthy() const noexcept {
    // Basic health check - all registered agents should be valid
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (!agents_[i]) {
            return false;
        }
    }
    return true;
}

Environment::EnvironmentStats Environment::get_stats() const noexcept {
    EnvironmentStats stats;
    stats.agent_count = agent_count_;
    stats.total_pending_messages = total_pending_messages();
    stats.is_healthy = is_healthy();
    
#if MINI_SO_ENABLE_METRICS
    stats.total_messages_sent = total_messages_sent_;
    stats.total_messages_processed = total_messages_processed_;
    stats.max_processing_time_us = max_processing_time_us_;
#else
    stats.total_messages_sent = 0;
    stats.total_messages_processed = 0;
    stats.max_processing_time_us = 0;
#endif
    
    return stats;
}

} // namespace mini_so