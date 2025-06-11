# Mini SObjectizer Headers

This directory contains the modular header files for Mini SObjectizer v3.0.

## Core Modules

### Primary Interface
- **`mini_sobjectizer.h`** - Main integration header (include this in your projects)
- **`mini_sobjectizer.h`** - Legacy monolithic header (for compatibility)

### Framework Components
- **`mini_so_config.h`** - Platform abstraction and build configuration
- **`mini_so_types.h`** - Core types and time utilities  
- **`mini_so_message.h`** - Message system with compile-time type safety
- **`mini_so_queue.h`** - High-performance cache-aligned message queue
- **`mini_so_agent.h`** - Agent framework and utilities
- **`mini_so_environment.h`** - Environment management and message routing
- **`mini_so_system_messages.h`** - System service message definitions

## Usage

For new projects, include the modular interface:

```cpp
#include "mini_sobjectizer/mini_sobjectizer.h"
```

For existing projects using the monolithic version:

```cpp
#include "mini_sobjectizer/mini_sobjectizer.h" 
```

Both interfaces provide identical functionality with zero performance overhead.