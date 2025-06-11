/**
 * @file agent_macros.h
 * @brief Agent definition and system initialization macros
 * 
 * Simplified macros for agent creation and system setup.
 */

#pragma once

namespace mini_so {

// Forward declarations
class Agent;
class Environment;
class System;

} // namespace mini_so

// ============================================================================
// Agent Definition Macros
// ============================================================================

#define MINI_SO_AGENT_BEGIN(ClassName) \
    class ClassName : public mini_so::Agent { \
    public: \
        bool handle_message(const mini_so::MessageBase& msg) noexcept override {

#define MINI_SO_AGENT_END() \
            return false; \
        } \
    };

// Simpler versions
#define DEFINE_AGENT(ClassName) MINI_SO_AGENT_BEGIN(ClassName)
#define END_AGENT() MINI_SO_AGENT_END()

// ============================================================================
// System Initialization Macros
// ============================================================================

#define MINI_SO_INIT() \
    mini_so::Environment& env = mini_so::Environment::instance(); \
    env.initialize()