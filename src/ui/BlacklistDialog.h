#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;

class BlacklistDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BlacklistDialog(QWidget* parent = nullptr);

private slots:
    void add();
    void removeSelected();
    void save();

private:
    QListWidget* m_list = nullptr;
    QLineEdit* m_appId = nullptr;
};
