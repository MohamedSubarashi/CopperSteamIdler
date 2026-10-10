#include "PowerManager.h"

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <powrprof.h>
#endif

namespace {

#ifdef Q_OS_WIN
// Enables a named privilege for the current process (needed for hibernation).
bool enablePrivilege(const wchar_t* name)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    if (!LookupPrivilegeValueW(nullptr, name, &privileges.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    const BOOL ok =
        AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
    const DWORD error = GetLastError();
    CloseHandle(token);
    return ok && error == ERROR_SUCCESS;
}
#endif

} // namespace

namespace PowerManager {

void preventSleep()
{
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
#endif
}

void allowSleep()
{
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

bool sleep()
{
#ifdef Q_OS_WIN
    return SetSuspendState(FALSE, FALSE, FALSE) != 0;
#else
    return false;
#endif
}

bool hibernate()
{
#ifdef Q_OS_WIN
    enablePrivilege(SE_SHUTDOWN_NAME);
    return SetSuspendState(TRUE, FALSE, FALSE) != 0;
#else
    return false;
#endif
}

} // namespace PowerManager
