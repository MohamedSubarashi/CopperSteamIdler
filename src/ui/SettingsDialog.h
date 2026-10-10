#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void openAdvanced();
    void openGames();
    void accept() override;

private:
    void loadSettings();

    QCheckBox* m_chkMinToTray = nullptr;
    QCheckBox* m_chkStartMinimized = nullptr;
    QCheckBox* m_chkIgnoreClient = nullptr;
    QCheckBox* m_chkShowUsername = nullptr;
    QCheckBox* m_chkDarkTheme = nullptr;
    QCheckBox* m_chkPreventSleep = nullptr;
    QCheckBox* m_chkIdleOnlyPlayed = nullptr;
    QComboBox* m_cboDoneAction = nullptr;
    QSpinBox* m_spnSimultaneous = nullptr;
    QDoubleSpinBox* m_spnTargetHours = nullptr;
    QComboBox* m_cboLanguage = nullptr;

    // Game selection (orthogonal to strategy).
    QRadioButton* m_radSelAll = nullptr;
    QRadioButton* m_radSelPicked = nullptr;
    QRadioButton* m_radSelLibrary = nullptr;

    // Idling strategy.
    QRadioButton* m_radFast = nullptr;
    QRadioButton* m_radOneGame = nullptr;
    QRadioButton* m_radManyThenOne = nullptr;
    QRadioButton* m_radOneThenMany = nullptr;

    QRadioButton* m_radOrderDefault = nullptr;
    QRadioButton* m_radOrderMost = nullptr;
    QRadioButton* m_radOrderLeast = nullptr;
};
