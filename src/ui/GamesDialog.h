#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;
class QRadioButton;

// Lets the user choose which games to idle (all card-eligible, a picked list,
// or the whole owned library) and edit the picked list.
class GamesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GamesDialog(QWidget* parent = nullptr);

private slots:
    void loadLibrary();
    void addManual();
    void removeSelected();

private:
    void save();
    void addEntry(const QString& appId, const QString& name, bool checked);

    QRadioButton* m_radAll = nullptr;
    QRadioButton* m_radPicked = nullptr;
    QRadioButton* m_radLibrary = nullptr;

    QListWidget* m_list = nullptr;
    QLineEdit* m_appId = nullptr;
    class QPushButton* m_btnRemove = nullptr;
};
