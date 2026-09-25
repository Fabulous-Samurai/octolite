cmake_minimum_required(VERSION 3.20)
project(octolite LANGUAGES CXX)

# Standardı C++26 seviyesine çekerek delete("reason") uyarılarını kökten çözüyoruz
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Derleme Seçeneği (Menü)
set(CHOICE "1" CACHE STRING "Derlenecek hedefi secin: 1=basic_ecs, 2=rotating_shapes, 3=benchmarks, 4=case_study, 5=all")

# Klasör Yolları
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)

set(CORE_SOURCES
    ${CORE_DIR}/octolite_allocator.cpp
    ${CORE_DIR}/octolite_chunk_pool.cpp
    ${CORE_DIR}/octolite_gpu.cpp
    ${CORE_DIR}/octolite_platform.cpp
    ${CORE_DIR}/octolite_registry.cpp
    ${CORE_DIR}/octolite_trace.cpp
    ${CORE_DIR}/octolite_window.cpp
    ${CORE_DIR}/vulkan_backend.cpp
)

add_library(octolite_core STATIC ${CORE_SOURCES})
target_include_directories(octolite_core PUBLIC ${INCLUDE_DIR})
target_include_directories(octolite_core PRIVATE ${SHADER_DIR})

# KESİN ÇÖZÜM: Vulkan'a Windows üzerinde çalıştığımızı bildiren global makroyu ekliyoruz
target_compile_definitions(octolite_core PUBLIC VK_USE_PLATFORM_WIN32_KHR)

find_package(Vulkan REQUIRED)
target_link_libraries(octolite_core PUBLIC Vulkan::Vulkan)

if(WIN32)
    target_link_libraries(octolite_core PUBLIC user32 gdi32 kernel32)
endif()

message(STATUS ">>>> Aktif Octolite Derleme Hedefi Secimi: ${CHOICE} <<<<")

if(CHOICE STREQUAL "1" OR CHOICE STREQUAL "5")
    add_executable(example_basic_ecs examples/basic_ecs/main.cpp)
    target_link_libraries(example_basic_ecs PRIVATE octolite_core)
    # Main projesine de Vulkan makrosunu enjekte ediyoruz
    target_compile_definitions(example_basic_ecs PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

if(CHOICE STREQUAL "2" OR CHOICE STREQUAL "5")
    add_executable(example_rotating_shapes examples/rotating_shapes/main.cpp)
    target_link_libraries(example_rotating_shapes PRIVATE octolite_core)
    target_compile_definitions(example_rotating_shapes PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

if(CHOICE STREQUAL "3" OR CHOICE STREQUAL "5")
    add_executable(bench_main benchmarks/main.cpp)
    target_link_libraries(bench_main PRIVATE octolite_core)
    target_compile_definitions(bench_main PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

if(CHOICE STREQUAL "4" OR CHOICE STREQUAL "5")
    add_executable(case_study_main case_study/main.cpp)
    target_link_libraries(case_study_main PRIVATE octolite_core)
    target_compile_definitions(case_study_main PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()
