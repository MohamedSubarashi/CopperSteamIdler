#pragma once

#include <QString>
#include <QtGlobal>

// Handles the Steam sign-in code that links an account for standalone farming,
// so cards can be farmed without the Steam client running.
//
// Steam refuses tokens minted for the website (their audience is web:*) for CM
// logon. A CM-compatible token only comes from a refresh token with a client
// audience, which is what the QR sign-in produces: the user scans a code with
// the Steam mobile app and Steam hands back a client refresh token. That token
// is stored and used to log on to the CM network; the short-lived access token
// needed for the community pages is then minted over that CM session.
class SteamAuth
{
public:
    struct QrChallenge {
        QString challengeUrl;
        QString clientId;
        QString requestId; // base64, as returned by Steam
        int intervalMs = 5000;
    };

    struct QrApproval {
        QString refreshToken;
        QString accessToken;
        QString accountName;
        quint64 steamId = 0;
    };

    enum class QrPoll { Pending, Approved, Failed };

    // True when a client refresh token is stored, i.e. the account has been
    // linked for standalone farming.
    static bool hasClientToken();

    // Starts a QR sign-in session. Blocking HTTPS request.
    static bool beginQrChallenge(QrChallenge* challenge, QString* error);

    // Polls the session once. The challenge is updated in place when Steam
    // hands out a new client id or sign-in URL while the code is on screen;
    // callers must re-render the QR code when challengeUrl changes. On
    // Approved, fills *approval and stores the tokens in the settings.
    static QrPoll pollQrChallenge(QrChallenge* challenge, QrApproval* approval,
                                  QString* error);

    // Forgets the linked client tokens (used when signing out).
    static void forgetClientToken();

    static quint64 steamIdFromSession();
    static quint64 steamIdFromToken(const QString& jwt);
    static qint64 tokenExpiry(const QString& jwt);
};
