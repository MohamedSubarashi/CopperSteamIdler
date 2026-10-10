#pragma once

#include <QString>

// Helpers for deriving the signed-in Steam profile from stored cookies.
namespace SteamProfile {

// SteamID64 extracted from steamLoginSecure (empty when unavailable).
QString steamId();

// https://steamcommunity.com/profiles/{steamid}
QString steamUrl();

// Human readable persona name (falls back to "User {steamid}").
QString signedAs();

} // namespace SteamProfile
