#pragma once

#include <QString>

// Read-only access to the *default* browser's cookie store so sign-in can reuse
// the session the user already has, without spawning a throwaway browser window.
//
// Only Firefox is readable this way: it keeps cookies unencrypted in
// cookies.sqlite. Modern Chrome/Edge encrypt cookies with App-Bound Encryption
// and refuse DevTools on the default profile, so they cannot be read here.
namespace BrowserCookies {

enum class Browser { Unknown, Firefox, Chrome, Edge, Brave };

// The Windows default browser for http/https (from the UserChoice registry key).
Browser defaultBrowser();
QString browserName(Browser browser);

// Directory of the active Firefox profile (empty when Firefox/its profile is
// missing). Reads profiles.ini and falls back to the newest profile that has a
// cookie database.
QString firefoxProfileDir();

// Returns the value of `cookieName` for a host containing `hostContains`, or an
// empty string. Reads a temporary copy so a running Firefox is never disturbed.
QString firefoxCookie(const QString& cookieName, const QString& hostContains);

} // namespace BrowserCookies
