#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

// Manual entry of the Steam Community cookie (steamLoginSecure). Only the web
// session lives here; farming without the Steam client uses the QR sign-in
// instead, because a website session cannot mint a client token.
class AdvancedDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AdvancedDialog(QWidget* parent = nullptr);

private slots:
    void toggleView();
    void update();
    void markDirty();

private:
    void setMasked(bool masked);

    QLineEdit* m_secure = nullptr;
    QPushButton* m_btnView = nullptr;
    QPushButton* m_btnUpdate = nullptr;
    bool m_masked = true;
};
