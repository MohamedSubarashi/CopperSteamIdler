#pragma once

#include <QString>
#include <QtGlobal>

// Turns the stored web session (steamLoginSecure) into a Steam access token that
// the CM protocol accepts for ClientLogon.
//
// The app signs in through the Steam website, so what it holds is a web session.
// Steam exposes a short-lived token for that session at /chat/clientjstoken,
// which is what the browser chat client itself uses; the CM accepts it.
class SteamAuth
{
public:
    struct Token {
        QString accessToken;
        quint64 steamId = 0;
        QString error;

        bool isValid() const { return !accessToken.isEmpty() && steamId != 0; }
    };

    // Blocking; performs one HTTPS request using the shared cookie jar.
    static Token fetchToken();

    // SteamID64 encoded in a steamLoginSecure value ("<steamid>||<jwt>").
    static quint64 steamIdFromSession();
};
