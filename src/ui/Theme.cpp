#include "Theme.h"

#include "../core/Settings.h"

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QMovie>
#include <QPalette>
#include <QScreen>
#include <QWidget>

namespace {

bool resourceExists(const QString& path)
{
    return QFile::exists(QStringLiteral(":") + path);
}

QString resolveIconPath(const QString& baseName)
{
    const bool white = Settings::instance().whiteIcons();
    const QString plain = QStringLiteral("/icons/%1.png").arg(baseName);
    if (white) {
        const QString whitePath = QStringLiteral("/icons/%1_w.png").arg(baseName);
        if (resourceExists(whitePath))
            return whitePath;
    }
    return plain;
}

QPalette makeDarkPalette()
{
    const QColor bgd = Settings::instance().colorBgd();
    const QColor txt = Settings::instance().colorTxt();

    QPalette p;
    p.setColor(QPalette::Window, bgd);
    p.setColor(QPalette::WindowText, txt);
    p.setColor(QPalette::Base, bgd.darker(110));
    p.setColor(QPalette::AlternateBase, bgd.darker(120));
    p.setColor(QPalette::ToolTipBase, bgd);
    p.setColor(QPalette::ToolTipText, txt);
    p.setColor(QPalette::Text, txt);
    p.setColor(QPalette::Button, bgd.lighter(115));
    p.setColor(QPalette::ButtonText, txt);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(126, 166, 75));
    p.setColor(QPalette::Highlight, QColor(126, 166, 75));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Disabled, QPalette::Text, txt.darker(150));
    p.setColor(QPalette::Disabled, QPalette::WindowText, txt.darker(150));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, txt.darker(150));
    return p;
}

} // namespace

namespace Theme {

QIcon icon(const QString& baseName)
{
    return QIcon(QStringLiteral(":") + resolveIconPath(baseName));
}

QIcon appIcon()
{
    return QIcon(QStringLiteral(":/app.ico"));
}

QIcon statusIcon(bool ok)
{
    return icon(ok ? QStringLiteral("check-2x") : QStringLiteral("x-2x"));
}

QMovie* spinner(QObject* parent)
{
    const bool white = Settings::instance().whiteIcons();
    const QString path = white ? QStringLiteral(":/icons/select2-spinner_inverted.gif")
                               : QStringLiteral(":/icons/select2-spinner.gif");
    QMovie* movie = new QMovie(path, QByteArray(), parent);
    movie->setScaledSize(QSize(14, 14));
    return movie;
}

void applyApplicationTheme(bool dark)
{
    static const QPalette original = QApplication::palette();
    if (dark)
        QApplication::setPalette(makeDarkPalette());
    else
        QApplication::setPalette(original);
}

void applyWidgetTheme(QWidget* widget, bool dark)
{
    if (!widget)
        return;
    if (!dark) {
        widget->setStyleSheet(QString());
        return;
    }
    const QString bgd = Settings::instance().colorBgd().name();
    const QString txt = Settings::instance().colorTxt().name();
    widget->setStyleSheet(QStringLiteral(
        "QWidget { background-color: %1; color: %2; }"
        "QMenuBar, QMenu { background-color: %1; color: %2; }"
        "QMenuBar::item:selected, QMenu::item:selected { background-color: %3; }"
        "QListView, QListWidget, QTreeView, QHeaderView::section {"
        "  background-color: %1; color: %2; }"
        "QPushButton { background-color: %3; color: %2; border: 1px solid %2; padding: 3px 8px; }"
        "QPushButton:hover { background-color: %4; }"
        "QLabel, QCheckBox, QRadioButton, QGroupBox { color: %2; }"
        "QLineEdit, QComboBox { background-color: %1; color: %2; border: 1px solid %2; }")
        .arg(bgd, txt, Settings::instance().colorBgd().lighter(130).name(),
             Settings::instance().colorBgd().lighter(150).name()));
}

void fitToScreen(QWidget* widget)
{
    if (!widget)
        return;

    QScreen* screen = widget->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect avail = screen->availableGeometry();
    const int maxW = qMax(320, int(avail.width() * 0.92));
    const int maxH = qMax(240, int(avail.height() * 0.92));

    // Prefer an explicitly set size; fall back to the layout hint while the
    // widget still has Qt's default 640x480 placeholder size.
    QSize target = widget->size();
    const bool placeholder = target.width() == 640 && target.height() == 480;
    if (target.width() <= 0 || target.height() <= 0 || placeholder)
        target = widget->sizeHint();
    target = target.expandedTo(widget->minimumSizeHint());

    const int w = qBound(280, target.width(), maxW);
    const int h = qBound(200, target.height(), maxH);

    widget->setMinimumSize(qMin(widget->minimumSize().width(), maxW),
                           qMin(widget->minimumSize().height(), maxH));
    widget->resize(w, h);
    widget->move(avail.center() - QPoint(w / 2, h / 2));
}

} // namespace Theme
