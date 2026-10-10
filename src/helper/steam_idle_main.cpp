// copper-steam-idle
//
// Minimal helper that boots the Steam client API for a single AppID so Steam
// reports the user as "in-game". It mirrors the original steam-idle.exe but
// loads the native steam_api64.dll at runtime, so no Steamworks SDK is needed.
//
// Usage: copper-steam-idle.exe <AppID>

#include <windows.h>

#include <ctype.h>
#include <stdlib.h>

namespace {

typedef bool (*SteamAPI_Init_t)();
typedef void (*SteamAPI_Shutdown_t)();
typedef void (*SteamAPI_RunCallbacks_t)();

void runCallbacks(SteamAPI_RunCallbacks_t fn)
{
    if (fn)
        fn();
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    if (!lpCmdLine || !*lpCmdLine)
        return 1;

    // First whitespace-separated token is the AppID.
    char buffer[64] = {0};
    int index = 0;
    for (const char* p = lpCmdLine; *p && !isspace(static_cast<unsigned char>(*p))
                                    && index < static_cast<int>(sizeof(buffer)) - 1;
         ++p) {
        buffer[index++] = *p;
    }
    buffer[index] = '\0';

    const long appId = strtol(buffer, nullptr, 10);
    if (appId <= 0)
        return 1;

    // SteamAPI_Init reads the SteamAppId environment variable.
    SetEnvironmentVariableA("SteamAppId", buffer);

    HMODULE steam = LoadLibraryA("steam_api64.dll");
    if (!steam)
        return 1;

    auto init = reinterpret_cast<SteamAPI_Init_t>(GetProcAddress(steam, "SteamAPI_Init"));
    auto shutdown =
        reinterpret_cast<SteamAPI_Shutdown_t>(GetProcAddress(steam, "SteamAPI_Shutdown"));
    auto callbacks =
        reinterpret_cast<SteamAPI_RunCallbacks_t>(GetProcAddress(steam, "SteamAPI_RunCallbacks"));

    if (!init || !init()) {
        FreeLibrary(steam);
        return 1;
    }

    // Keep the "in-game" state alive until the parent kills this process.
    for (;;) {
        runCallbacks(callbacks);
        Sleep(100);
    }

    (void)shutdown; // unreachable; the parent terminates the process
    return 0;
}
