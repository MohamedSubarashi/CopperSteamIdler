# Copper Steam Idler

A Qt 6 / C++ port of **Idle Master Extended** — idles your Steam games to earn
trading card drops the quickest possible way.

This is a faithful reimplementation of the original .NET WinForms app
(`idle_master_extended` by jshackles / JonasNilson) using Qt Widgets. It keeps
the same workflow, settings, idling modes and (best-effort) the same Steam
Community scraping behaviour, while replacing the bundled .NET `steam-idle.exe`
helper with a small native C++ helper.

> **Requires** the Steam client to be running and signed in. Card drop data is
> read from the Steam Community website using your saved login cookies.

## Download

Pre-built Windows binaries are published on the
[Releases](https://github.com/MohamedSubarashi/CopperSteamIdler/releases) page.
Documentation and help live on the
[Wiki](https://github.com/MohamedSubarashi/CopperSteamIdler/wiki).

- Repository: <https://github.com/MohamedSubarashi/CopperSteamIdler>
- Steam group: <https://steamcommunity.com/groups/coppersteamidler>
- Support / donate: <https://ko-fi.com/mohamedsubarashi>

## How it works

Two executables are produced:

| Executable | Role |
| --- | --- |
| `CopperSteamIdler.exe` | The Qt GUI. Signs in to the Steam Community, scrapes your badge list, and drives the idle loop. |
| `copper-steam-idle.exe` | A tiny helper. It sets `SteamAppId` and calls `SteamAPI_Init()` (loaded from `steam_api64.dll`), which makes Steam report you "in-game" for that AppID. One helper runs per idled game. |

There is no IPC: the GUI simply launches/kills one helper process per AppID.
Because `SteamAPI_Init` is resolved from the DLL at runtime via
`GetProcAddress`, **no Steamworks SDK is required** — only `steam_api64.dll`
(shipped in `libs/`).

## Building

Requirements (already present on the reference machine):

- Qt **6.12.0** (MinGW 64-bit) with the `Widgets`, `Network`, `Svg` and `Sql`
  modules
- MinGW GCC **13.1** (`C:\Qt\Tools\mingw1310_64`)
- CMake **3.21+** and Ninja

Sign In uses your **default browser** and needs no extra Qt module: for Firefox
(cookies readable) the login page opens in the browser you already use and the
`steamLoginSecure` cookie is captured from `cookies.sqlite` automatically.
Chrome/Edge encrypt their cookies (App-Bound Encryption), so for those an
optional DevTools sign-in window is offered. Qt WebEngine is optional and, when
present, is used instead (embedded browser). Manual cookie entry is always
available.

```powershell
cmake --preset mingw
cmake --build --preset mingw
```

The binaries are written to `build/mingw/`. To produce a self-contained folder
with the Qt runtime, either run the helper target:

```powershell
cmake --build --preset mingw --target deploy
```

or add Qt's `bin` directory to `PATH` before launching.

## Configuration

Settings are stored as an INI file:

```
%LOCALAPPDATA%\Copper Steam Idler\CopperSteamIdler\settings.ini
```

Diagnostics are appended to `error.log` next to the executable.

### Signing in

Logging in now uses a single value — the Steam Community
`steamLoginSecure` cookie. The old `sessionid` / `steamparental` cookies are no
longer required and are stripped automatically when found.

- **Quick Login (recommended)** — choose **File → Sign In…**. The Steam login
  page opens in your **default browser** (no separate app window). Firefox's
  `cookies.sqlite` is polled and the `steamLoginSecure` cookie is captured,
  validated and saved automatically once you are signed in. For a
  Chromium-based default browser an optional DevTools sign-in window is offered.
- **Manual entry** — pasting the `steamLoginSecure` value is always available
  via **Settings → Advanced**; it is validated against the Steam Community
  before being saved.

The stored session is validated by fetching
`/profiles/<steamid64>/?xml=1` and checking the returned SteamID matches the
cookie. Invalid stored sessions are refreshed (via `login.steampowered.com`)
before the user is prompted to sign in again.

## Feature overview

- Steam-running and login status indicators.
- Automatic **Sign In** using your default browser (Firefox cookie capture), with
  a DevTools sign-in window for Chromium and an embedded WebEngine browser used
  when available.
- Badge list scraping with blacklist support.
- **Game selection** (independent of strategy): all card-eligible games, a
  hand-picked list, or your **whole Steam library**.
- **Idling strategies**: fast, one game at a time, many-then-one,
  one-then-many; up to 30 simultaneous games.
- **Custom simultaneous idling duration** (default 6 minutes).
- **Time-played goals**: optionally idle each game until it reaches a target
  number of hours, even after card drops run out.
- Pause / resume / skip, "blacklist current game", per-game drop counters and a
  progress bar.
- Manual drop-check trigger (click the "card drops remaining" text).
- **Start minimized** to the system tray, resizable window with saved size,
  dark theme with white icon variants, minimize-to-tray and prevent-sleep.
- When idling completes: do nothing, **shut down**, **sleep** or **hibernate**.
- Session + lifetime statistics.
- **Interface languages**: English, Français, Español, Português (Brasil) and
  العربية (full right-to-left mirroring), plus *System default* which follows
  the OS locale.

## Robustness notes

Several long-standing upstream issues are addressed at the application level:

- A network / HTTP failure while checking card drops is **never** treated as
  "no drops left"; the game is retried instead of being skipped.
- Steam Community requests are throttled and backed off when rate limiting is
  detected.
- A missing/unavailable game banner shows a placeholder instead of aborting the
  idle loop.
- Helper processes are monitored and restarted if they exit unexpectedly.
- Unhandled exceptions in the event loop are logged instead of crashing the app
  (`error.log`).
- Only one instance runs at a time.

## Translations

All user-facing strings use `tr()`. The interface is available in:

| Language | Code | Catalogue |
| --- | --- | --- |
| English | `en` | `i18n/copper_en.ts` |
| Français | `fr` | `i18n/copper_fr.ts` |
| Español | `es` | `i18n/copper_es.ts` |
| Português (Brasil) | `pt_BR` | `i18n/copper_pt_BR.ts` |
| العربية (RTL) | `ar` | `i18n/copper_ar.ts` |

Pick **Settings → Interface Language**. The first entry, *System default*,
follows the operating system locale; changing the language requires a restart.

English is the source language, so `copper_en.ts` simply repeats the sources —
it exists so a user who forces `en` sees identical text regardless of the OS
locale.

App catalogues are compiled by `qt_add_translations` and embedded in the
executable under the Qt resource prefix `:/i18n`, so they need no files on
disk. Qt's own catalogue (`qtbase_*.qm`, standard button labels and context
menus) is *not* bundled by `windeployqt --no-translations`, so the
`translations/` folder next to the executable is shipped with the
`qtbase_{en,es,fr,pt_BR,ar}.qm` files and loaded at startup.

To add a language:

```powershell
lupdate src -ts i18n/copper_de.ts
```

Translate the `.ts`, add the language code to the combo box in
`src/ui/SettingsDialog.cpp`, and drop the matching `qtbase_de.qm` from
`C:\Qt\<version>\mingw_64\translations\` into the shipped `translations/`
folder. Then build — every `i18n/*.ts` file is compiled to a `.qm`
automatically.

## Directory layout

```
src/core/    settings, logging, HTTP + cookies, HTML scraping, processes
src/ui/      main window, dialogs, theme
src/helper/  the native idle helper
resources/   app icon, Open Iconic assets, Qt resource file
libs/        steam_api64.dll
i18n/        translation catalogues
```

## Credits & license

Copper Steam Idler is maintained by
[MohamedSubarashi](https://github.com/MohamedSubarashi). It is a port of
**Idle Master Extended** by [jshackles](https://github.com/jshackles) and
[JonasNilson](https://github.com/JonasNilson), itself based on the original
Idle Master. Icons are from [Open Iconic](https://github.com/iconic/open-iconic).

Licensed under the **GNU General Public License v2** (see `LICENSE`). See
`THIRD_PARTY_NOTICES.md` for third-party components.
