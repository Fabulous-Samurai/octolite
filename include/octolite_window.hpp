#pragma once
#include "pch.hpp"

namespace octolite {

// Tür ismini çakışmaları önlemek için OctoWindow olarak güncelledik
struct OctoWindow {
    HWND hwnd{nullptr};
    uint32_t width{0};
    uint32_t height{0};
    bool quit{false};
};

OctoWindow create_window(uint32_t width, uint32_t height, const wchar_t* title);
void destroy_window(OctoWindow& win);
bool pump_messages(OctoWindow& win);

} // namespace octolite
