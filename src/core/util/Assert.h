#pragma once

#include "Log.h"
#include <cstdlib>

// ─── Assertions that log before crashing ───
#define GODSIM_ASSERT(condition, ...)                                       \
    do {                                                                    \
        if (!(condition)) {                                                 \
            LOG_CRITICAL("ASSERTION FAILED: {} ({}:{})", #condition,        \
                         __FILE__, __LINE__);                               \
            LOG_CRITICAL(__VA_ARGS__);                                      \
            std::abort();                                                   \
        }                                                                   \
    } while (0)

#ifdef NDEBUG
    #define GODSIM_DEBUG_ASSERT(condition, ...) ((void)0)
#else
    #define GODSIM_DEBUG_ASSERT(condition, ...) GODSIM_ASSERT(condition, __VA_ARGS__)
#endif

// ─── Unreachable code marker ───
#define GODSIM_UNREACHABLE(...)                                             \
    do {                                                                    \
        LOG_CRITICAL("UNREACHABLE CODE REACHED ({}:{})", __FILE__, __LINE__); \
        LOG_CRITICAL(__VA_ARGS__);                                          \
        std::abort();                                                       \
    } while (0)
