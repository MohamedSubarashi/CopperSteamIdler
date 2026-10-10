#include "StatisticsDialog.h"

#include "../core/Settings.h"
#include "../core/Statistics.h"
#include "Theme.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

StatisticsDialog::StatisticsDialog(const Statistics& statistics, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Statistics"));
    setWindowIcon(Theme::appIcon());
    setModal(true);

    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(tr("This Session"), this));
    root->addWidget(new QLabel(statistics.sessionTimeText(), this));
    root->addWidget(new QLabel(statistics.sessionCardsText(), this));

    root->addSpacing(12);
    root->addWidget(new QLabel(tr("Total"), this));

    const uint totalMinutes = Settings::instance().totalMinutesIdled();
    root->addWidget(new QLabel(tr("%1 hours, %2 minutes idled")
                                   .arg(totalMinutes / 60)
                                   .arg(totalMinutes % 60),
                               this));
    root->addWidget(new QLabel(tr("%1 cards idled").arg(Settings::instance().totalCardIdled()), this));

    auto* btnOk = new QPushButton(tr("&OK"), this);
    connect(btnOk, &QPushButton::clicked, this, &StatisticsDialog::accept);
    root->addWidget(btnOk);

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);
}
