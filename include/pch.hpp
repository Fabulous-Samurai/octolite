#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows ana header
#include <windows.h>

// Eksik alt header'ları explicit olarak ekle
#include <winbase.h>
#include <securitybaseapi.h>
#include <timezoneapi.h>
#include <winerror.h>
#include <winnls.h>

// Fallback: Eğer LPOVERLAPPED_COMPLETION_ROUTINE hala tanımsızsa manuel tanımla
#ifndef _LPOVERLAPPED_COMPLETION_ROUTINE_DEFINED
typedef VOID (WINAPI *LPOVERLAPPED_COMPLETION_ROUTINE)(
    DWORD dwErrorCode,
    DWORD dwNumberOfBytesTransfered,
    LPOVERLAPPED lpOverlapped
);
#define _LPOVERLAPPED_COMPLETION_ROUTINE_DEFINED
#endif

#ifdef OCTOLITE_ENABLE_GPU
    #define VK_USE_PLATFORM_WIN32_KHR
    #include <vulkan/vulkan.h>
#endif

// C++ Standard Library
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
#include <array>
#include <expected>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <type_traits>
