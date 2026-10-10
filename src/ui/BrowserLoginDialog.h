#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QTimer;

// Sign in using the user's *default* browser instead of spawning a throwaway
// browser window.
//
// For Firefox (whose cookies are readable) the Steam login page is opened in
// the existing browser and cookies.sqlite is polled until steamLoginSecure
// appears; the cookie is then captured automatically. For Chromium-based
// browsers (cookies App-Bound-Encrypted) it offers the DevTools sign-in window
// or manual entry instead.
class BrowserLoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BrowserLoginDialog(QWidget* parent = nullptr);

    QString capturedCookie() const { return m_cookie; }
    bool manualRequested() const { return m_manualRequested; }

private slots:
    void openLoginPage();
    void openSignInWindow();
    void pollCookie();
    void useManualEntry();

private:
    void setStatus(const QString& text);

    QLabel* m_status = nullptr;
    QPushButton* m_btnOpen = nullptr;
    QPushButton* m_btnWindow = nullptr;
    QPushButton* m_btnManual = nullptr;
    QTimer* m_poll = nullptr;

    int m_elapsedMs = 0;
    bool m_polling = false;
    bool m_manualRequested = false;
    QString m_cookie;
};
