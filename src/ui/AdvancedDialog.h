#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

// Manual entry of the single Steam Community cookie that is actually needed:
// steamLoginSecure.
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
