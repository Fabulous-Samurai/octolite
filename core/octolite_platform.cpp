#include "../include/pch.hpp"
#include "octolite_platform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Başlık dosyalarının tam ve doğru sırada dahil edildiğinden emin oluyoruz
#include <windows.h>
#include <winbase.h>
#include <securitybaseapi.h>

namespace octolite {

bool enable_lock_memory_privilege() noexcept {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }

    LUID luid{};

    // SeLockMemoryPrivilege ayrıcalığının LUID değerini alıyoruz
    if (!LookupPrivilegeValueW(nullptr, L"SeLockMemoryPrivilege", &luid)) {
        CloseHandle(token);
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Ayrıcalığı aktif etmeyi deniyoruz
    bool result = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr) != 0;

    // Windows, ayrıcalık kullanıcı grubunda tanımlı değilse AdjustTokenPrivileges başarılı olsa bile
    // ERROR_NOT_ALL_ASSIGNED hatası döner. Bunu yakalamak şarttır.
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        result = false;
    }

    CloseHandle(token);
    return result;
}

} // namespace octolite
