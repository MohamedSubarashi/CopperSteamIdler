#pragma once

#include <QDialog>
#include <QElapsedTimer>

#include "../core/SteamAuth.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

// Links the Steam account for standalone farming by showing a QR code that the
// user approves in the Steam mobile app. Steam then hands back a client refresh
// token, which is what the CM protocol accepts; website cookies never can.
class QrLoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QrLoginDialog(QWidget* parent = nullptr);

private slots:
    void beginChallenge();
    void poll();
    void copyLink();
    void unlink();

private:
    void setStatus(const QString& text);
    void showChallenge(const SteamAuth::QrChallenge& challenge);
    void stopPolling();

    QLabel* m_qr = nullptr;
    QLineEdit* m_link = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_btnCopy = nullptr;
    QPushButton* m_btnRetry = nullptr;
    QPushButton* m_btnUnlink = nullptr;
    QPushButton* m_btnClose = nullptr;
    QTimer* m_poll = nullptr;
    QElapsedTimer m_elapsed;

    SteamAuth::QrChallenge m_challenge;
};
