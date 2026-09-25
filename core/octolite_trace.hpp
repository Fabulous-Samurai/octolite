#pragma once
#include "../include/pch.hpp"

#ifdef OCTOLITE_TRACE_ENABLED
    #define OCTOLITE_TRACE_INSTANT(name) \
        do { \
            static thread_local uint64_t _trace_ts = 0; \
            QueryPerformanceCounter((LARGE_INTEGER*)&_trace_ts); \
            printf("[TRACE] %s @ %llu\n", name, _trace_ts); \
        } while(0)
#else
    #define OCTOLITE_TRACE_INSTANT(name) ((void)0)
#endif
