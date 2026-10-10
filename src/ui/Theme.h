#pragma once

#include <QIcon>
#include <QString>

class QWidget;
class QMovie;
class QObject;

// Icon + palette helpers. Icon names follow the bundled Open Iconic files,
// e.g. icon("ban-2x") resolves to :/icons/ban-2x.png (or the _w white variant
// when the white-icon theme is active).
namespace Theme {

QIcon icon(const QString& baseName);
QIcon appIcon();

// check-2x / x-2x status icon.
QIcon statusIcon(bool ok);

// Returns a new QMovie for the spinner appropriate to the current theme.
QMovie* spinner(QObject* parent);

// Applies or removes the dark palette on the whole application.
void applyApplicationTheme(bool dark);

// Sets the widget (and children) background/text to the custom theme colours.
void applyWidgetTheme(QWidget* widget, bool dark);

// Caps a dialog/window to ~92% of the available screen and centres it, so it
// never opens larger than the display.
void fitToScreen(QWidget* widget);

} // namespace Theme
