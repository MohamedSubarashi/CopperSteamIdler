#include "SettingsDialog.h"

#include "../core/Settings.h"
#include "AdvancedDialog.h"
#include "GamesDialog.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Idle Master Settings"));
    setWindowIcon(Theme::appIcon());
    setModal(true);

    auto* root = new QVBoxLayout(this);

    // General --------------------------------------------------------------
    auto* general = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QVBoxLayout(general);
    m_chkMinToTray = new QCheckBox(tr("Minimize Idle Master to system tray"), general);
    m_chkStartMinimized = new QCheckBox(tr("Start minimized to the system tray"), general);
    m_chkIgnoreClient = new QCheckBox(tr("Ignore Steam client status"), general);
    m_chkShowUsername = new QCheckBox(tr("Show Steam username of signed on user"), general);
    m_chkDarkTheme = new QCheckBox(tr("Dark theme"), general);
    m_chkPreventSleep = new QCheckBox(tr("Prevent computer from sleeping"), general);
    m_chkIdleOnlyPlayed = new QCheckBox(tr("Only idle games that have been played"), general);
    generalLayout->addWidget(m_chkMinToTray);
    generalLayout->addWidget(m_chkStartMinimized);
    generalLayout->addWidget(m_chkIgnoreClient);
    generalLayout->addWidget(m_chkShowUsername);
    generalLayout->addWidget(m_chkDarkTheme);
    generalLayout->addWidget(m_chkPreventSleep);
    generalLayout->addWidget(m_chkIdleOnlyPlayed);

    auto* doneRow = new QHBoxLayout;
    doneRow->addWidget(new QLabel(tr("When idling completes"), general));
    m_cboDoneAction = new QComboBox(general);
    m_cboDoneAction->addItem(tr("Do nothing"), 0);
    m_cboDoneAction->addItem(tr("Shut down the computer"), 1);
    m_cboDoneAction->addItem(tr("Sleep"), 2);
    m_cboDoneAction->addItem(tr("Hibernate"), 3);
    doneRow->addWidget(m_cboDoneAction);
    doneRow->addStretch();
    generalLayout->addLayout(doneRow);

    auto* langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(tr("Interface Language"), general));
    m_cboLanguage = new QComboBox(general);
    // Language code -> native name. An empty code means "follow the operating
    // system", which is also what an unset setting resolves to.
    m_cboLanguage->addItem(tr("System default"), QStringLiteral(""));
    m_cboLanguage->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_cboLanguage->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_cboLanguage->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    m_cboLanguage->addItem(QStringLiteral("Português (Brasil)"), QStringLiteral("pt_BR"));
    m_cboLanguage->addItem(QStringLiteral("العربية"), QStringLiteral("ar"));
    langRow->addWidget(m_cboLanguage);
    langRow->addStretch();
    generalLayout->addLayout(langRow);

    auto* langNote = new QLabel(tr("A restart is required to apply a language change."), general);
    langNote->setWordWrap(true);
    langNote->setEnabled(false);
    generalLayout->addWidget(langNote);

    // Game selection -------------------------------------------------------
    auto* selection = new QGroupBox(tr("Games to Idle"), this);
    auto* selectionLayout = new QVBoxLayout(selection);
    m_radSelAll = new QRadioButton(tr("All games with card drops remaining"), selection);
    m_radSelPicked = new QRadioButton(tr("Only the games I pick"), selection);
    m_radSelLibrary = new QRadioButton(tr("My whole Steam library"), selection);
    selectionLayout->addWidget(m_radSelAll);
    selectionLayout->addWidget(m_radSelPicked);
    selectionLayout->addWidget(m_radSelLibrary);
    auto* pickRow = new QHBoxLayout;
    auto* btnGames = new QPushButton(tr("Select Games..."), selection);
    btnGames->setIcon(Theme::icon(QStringLiteral("grid-three-up-2x")));
    pickRow->addWidget(btnGames);
    pickRow->addStretch();
    selectionLayout->addLayout(pickRow);

    // Idling strategy ------------------------------------------------------
    auto* behavior = new QGroupBox(tr("Idling Strategy"), this);
    auto* behaviorLayout = new QVBoxLayout(behavior);
    m_radFast = new QRadioButton(tr("Fast mode (Recommended)"), behavior);
    m_radOneGame = new QRadioButton(tr("Idle each game individually"), behavior);
    m_radManyThenOne =
        new QRadioButton(tr("Idle games simultaneously up to 2 hours, then individually"), behavior);
    m_radOneThenMany =
        new QRadioButton(tr("Idle games with more than 2 hours individually, then simultaneously"),
                         behavior);
    behaviorLayout->addWidget(m_radFast);
    behaviorLayout->addWidget(m_radOneGame);
    behaviorLayout->addWidget(m_radManyThenOne);
    behaviorLayout->addWidget(m_radOneThenMany);

    auto* durationRow = new QHBoxLayout;
    durationRow->addWidget(new QLabel(tr("Simultaneous idling duration (minutes)"), behavior));
    m_spnSimultaneous = new QSpinBox(behavior);
    m_spnSimultaneous->setRange(1, 720);
    durationRow->addWidget(m_spnSimultaneous);
    durationRow->addStretch();
    behaviorLayout->addLayout(durationRow);

    // Time goals -----------------------------------------------------------
    auto* goals = new QGroupBox(tr("Time Played Goals"), this);
    auto* goalsForm = new QFormLayout(goals);
    m_spnTargetHours = new QDoubleSpinBox(goals);
    m_spnTargetHours->setRange(0.0, 10000.0);
    m_spnTargetHours->setDecimals(1);
    m_spnTargetHours->setSpecialValueText(tr("Disabled"));
    goalsForm->addRow(tr("Idle each game up to (hours)"), m_spnTargetHours);

    // Idling order ---------------------------------------------------------
    auto* order = new QGroupBox(tr("Idling Order"), this);
    auto* orderLayout = new QVBoxLayout(order);
    m_radOrderDefault = new QRadioButton(tr("Default (Alphabetical Order)"), order);
    m_radOrderMost =
        new QRadioButton(tr("Prioritize games with the highest number of available drops"), order);
    m_radOrderLeast =
        new QRadioButton(tr("Prioritize games with the lowest number of available drops"), order);
    orderLayout->addWidget(m_radOrderDefault);
    orderLayout->addWidget(m_radOrderMost);
    orderLayout->addWidget(m_radOrderLeast);

    // Two-column grid inside a scroll area so the dialog always fits the
    // display, even on small screens.
    auto* content = new QWidget;
    auto* grid = new QGridLayout(content);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    grid->addWidget(general, 0, 0);
    grid->addWidget(selection, 0, 1);
    grid->addWidget(behavior, 1, 0);
    grid->addWidget(goals, 1, 1);
    grid->addWidget(order, 2, 0, 1, 2);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(2, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // Buttons --------------------------------------------------------------
    auto* buttonRow = new QHBoxLayout;
    auto* btnAdvanced = new QPushButton(tr("Advanced"), this);
    btnAdvanced->setIcon(Theme::icon(QStringLiteral("lock-locked-2x")));
    auto* btnOk = new QPushButton(tr("&Accept"), this);
    auto* btnCancel = new QPushButton(tr("&Cancel"), this);
    buttonRow->addWidget(btnAdvanced);
    buttonRow->addStretch();
    buttonRow->addWidget(btnOk);
    buttonRow->addWidget(btnCancel);
    root->addLayout(buttonRow);

    connect(btnAdvanced, &QPushButton::clicked, this, &SettingsDialog::openAdvanced);
    connect(btnGames, &QPushButton::clicked, this, &SettingsDialog::openGames);
    connect(btnOk, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &SettingsDialog::reject);

    loadSettings();

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    // Open with a roomy two-column layout so the settings use the available
    // space; the scroll area still handles small/laptop screens.
    setMinimumSize(720, 480);
    resize(1000, 600);

    Theme::fitToScreen(this);
}

void SettingsDialog::loadSettings()
{
    Settings& s = Settings::instance();

    m_chkMinToTray->setChecked(s.minToTray());
    m_chkStartMinimized->setChecked(s.startMinimized());
    m_chkIgnoreClient->setChecked(s.ignoreClient());
    m_chkShowUsername->setChecked(s.showUsername());
    m_chkDarkTheme->setChecked(s.customTheme());
    m_chkPreventSleep->setChecked(s.noSleep());
    m_chkIdleOnlyPlayed->setChecked(s.idleOnlyPlayed());

    m_cboDoneAction->setCurrentIndex(m_cboDoneAction->findData(s.doneAction()));
    m_spnSimultaneous->setValue(int(s.simultaneousMinutes()));
    m_spnTargetHours->setValue(s.targetHours());

    const int langIndex = m_cboLanguage->findData(s.language());
    m_cboLanguage->setCurrentIndex(langIndex >= 0 ? langIndex : 0);

    const QString selection = s.selectionMode();
    if (selection == QLatin1String("picked"))
        m_radSelPicked->setChecked(true);
    else if (selection == QLatin1String("library"))
        m_radSelLibrary->setChecked(true);
    else
        m_radSelAll->setChecked(true);

    if (s.onlyOneGameIdle())
        m_radOneGame->setChecked(true);
    else if (s.oneThenMany())
        m_radOneThenMany->setChecked(true);
    else if (s.fastMode())
        m_radFast->setChecked(true);
    else
        m_radManyThenOne->setChecked(true);

    const QString sort = s.sort();
    if (sort == QLatin1String("mostcards"))
        m_radOrderMost->setChecked(true);
    else if (sort == QLatin1String("leastcards"))
        m_radOrderLeast->setChecked(true);
    else
        m_radOrderDefault->setChecked(true);
}

void SettingsDialog::openAdvanced()
{
    AdvancedDialog dialog(this);
    dialog.exec();
}

void SettingsDialog::openGames()
{
    GamesDialog dialog(this);
    dialog.exec();
}

void SettingsDialog::accept()
{
    Settings& s = Settings::instance();

    s.setMinToTray(m_chkMinToTray->isChecked());
    s.setStartMinimized(m_chkStartMinimized->isChecked());
    s.setIgnoreClient(m_chkIgnoreClient->isChecked());
    s.setShowUsername(m_chkShowUsername->isChecked());
    s.setCustomTheme(m_chkDarkTheme->isChecked());
    s.setWhiteIcons(m_chkDarkTheme->isChecked());
    s.setNoSleep(m_chkPreventSleep->isChecked());
    s.setIdleOnlyPlayed(m_chkIdleOnlyPlayed->isChecked());
    s.setLanguage(m_cboLanguage->currentData().toString());
    s.setSimultaneousMinutes(uint(m_spnSimultaneous->value()));
    s.setTargetHours(m_spnTargetHours->value());

    const int done = m_cboDoneAction->currentData().toInt();
    if (done == 1 && s.doneAction() != 1) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Copper Steam Idler"),
            tr("Are you sure you want your computer to shut down when idling completes?"));
        s.setDoneAction(answer == QMessageBox::Yes ? 1 : 0);
    } else {
        s.setDoneAction(done);
    }

    if (m_radSelPicked->isChecked())
        s.setSelectionMode(QStringLiteral("picked"));
    else if (m_radSelLibrary->isChecked())
        s.setSelectionMode(QStringLiteral("library"));
    else
        s.setSelectionMode(QStringLiteral("all"));

    // Exactly one idling strategy.
    s.setFastMode(false);
    s.setOneThenMany(false);
    s.setOnlyOneGameIdle(false);
    if (m_radOneGame->isChecked())
        s.setOnlyOneGameIdle(true);
    else if (m_radOneThenMany->isChecked())
        s.setOneThenMany(true);
    else if (m_radFast->isChecked())
        s.setFastMode(true);
    // else many-then-one (all flags false)

    if (m_radOrderMost->isChecked())
        s.setSort(QStringLiteral("mostcards"));
    else if (m_radOrderLeast->isChecked())
        s.setSort(QStringLiteral("leastcards"));
    else
        s.setSort(QStringLiteral("default"));

    s.save();
    QDialog::accept();
}
