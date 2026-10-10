#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QNetworkCookie;
class QPushButton;
class QWebEngineProfile;
class QWebEngineView;

// Embedded Steam login. Uses a temporary (off-the-record) browser profile so
// nothing is persisted outside the app, watches for the steamLoginSecure cookie
// and hands it back to the caller once the user signs in.
class QuickLoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QuickLoginDialog(QWidget* parent = nullptr);
    ~QuickLoginDialog() override;

    // The captured steamLoginSecure value (empty when cancelled).
    QString capturedCookie() const { return m_cookie; }

private slots:
    void onCookieAdded(const QNetworkCookie& cookie);
    void onLoadFinished(bool ok);
    void probeCookies();

private:
    QWebEngineProfile* m_profile = nullptr;
    QWebEngineView* m_view = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_btnDone = nullptr;
    QString m_cookie;
};
