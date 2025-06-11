/**
 * @file config.h
 * @brief Configuration definitions for Mini SObjectizer
 * 
 * Compile-time configuration parameters for performance tuning
 * and feature selection.
 */

#pragma once

// ============================================================================
// Core System Configuration
// ============================================================================

#ifndef MINI_SO_MAX_AGENTS
#define MINI_SO_MAX_AGENTS 16
#endif

#ifndef MINI_SO_MAX_QUEUE_SIZE
#define MINI_SO_MAX_QUEUE_SIZE 64
#endif

#ifndef MINI_SO_MAX_MESSAGE_SIZE
#define MINI_SO_MAX_MESSAGE_SIZE 128
#endif

// ============================================================================
// Feature Configuration
// ============================================================================

#ifndef MINI_SO_ENABLE_METRICS
#define MINI_SO_ENABLE_METRICS 1
#endif

#ifndef MINI_SO_ENABLE_VALIDATION
#define MINI_SO_ENABLE_VALIDATION 1
#endif