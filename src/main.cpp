#include "core/Logger.h"
#include "core/PowerManager.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QLockFile>
#include <QMessageBox>
#include <QTranslator>

#include <exception>

namespace {

// Catches exceptions thrown from event handlers so a single bad scrape can
// never take the whole app down (the original crashed with 0xe0434352).
class GuardedApplication : public QApplication
{
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception& ex) {
            Logger::exception(QStringLiteral("Unhandled exception in event loop"),
                              QString::fromUtf8(ex.what()));
        } catch (...) {
            Logger::exception(QStringLiteral("Unhandled exception in event loop"),
                              QStringLiteral("unknown"));
        }
        return false;
    }
};

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    switch (type) {
    case QtDebugMsg:
        Logger::log(QStringLiteral("[debug] %1").arg(message));
        break;
    case QtInfoMsg:
        Logger::log(QStringLiteral("[info] %1").arg(message));
        break;
    case QtWarningMsg:
        Logger::log(QStringLiteral("[warning] %1").arg(message));
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        Logger::error(QStringLiteral("[critical] %1").arg(message));
        break;
    }
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef QUICK_LOGIN_ENABLED
    // Required by Qt WebEngine before the QApplication is constructed.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif
    GuardedApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Copper Steam Idler"));
    QCoreApplication::setApplicationName(QStringLiteral("CopperSteamIdler"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0"));
    QApplication::setQuitOnLastWindowClosed(false);

    qInstallMessageHandler(messageHandler);

    // Migrate older settings layouts once.
    Settings::instance().upgrade();

    const QString appDir = QCoreApplication::applicationDirPath();

    // i18n: restore the saved interface language (an empty setting means
    // "follow the operating system"). QTranslator's QLocale overload handles
    // the usual fallbacks (fr_FR -> fr, pt_BR -> pt) that a plain file-name
    // lookup would miss.
    const QString language = Settings::instance().language();
    const QLocale locale(language.isEmpty() ? QLocale::system().name() : language);
    QLocale::setDefault(locale);
    // Right-to-left languages (Arabic) must mirror the whole interface.
    QApplication::setLayoutDirection(locale.textDirection());

    QTranslator appTranslator;
    const QStringList appI18nDirs{appDir + QStringLiteral("/i18n"), QStringLiteral(":/i18n")};
    for (const QString& dir : appI18nDirs) {
        if (appTranslator.load(locale, QStringLiteral("copper"), QStringLiteral("_"), dir)) {
            app.installTranslator(&appTranslator);
            Logger::log(QStringLiteral("Loaded application translations for '%1'.")
                            .arg(locale.name()));
            break;
        }
    }

    // Qt's own catalogues (standard dialog buttons, text-field context menus,
    // ...) are not shipped by windeployqt --no-translations, so look next to
    // the executable first and fall back to the Qt installation when running
    // from a development build.
    QTranslator qtTranslator;
    const QStringList qtTranslationDirs{appDir + QStringLiteral("/translations"),
                                        QLibraryInfo::path(QLibraryInfo::TranslationsPath)};
    for (const QString& dir : qtTranslationDirs) {
        if (qtTranslator.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), dir)) {
            app.installTranslator(&qtTranslator);
            break;
        }
    }

    // Avoid two instances fighting over the same helper processes.
    QLockFile lock(Settings::dataDirectory() + QStringLiteral("/copper.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100)) {
        QMessageBox::information(nullptr, QStringLiteral("Copper Steam Idler"),
                                 QObject::tr("Copper Steam Idler is already running."));
        return 0;
    }

    // Verify runtime dependencies next to the executable.
    QStringList missing;
    if (!QFileInfo::exists(appDir + QStringLiteral("/copper-steam-idle.exe")))
        missing << QStringLiteral("copper-steam-idle.exe");
    if (!QFileInfo::exists(appDir + QStringLiteral("/steam_api64.dll")))
        missing << QStringLiteral("steam_api64.dll");
    if (!missing.isEmpty()) {
        QMessageBox::critical(
            nullptr, QStringLiteral("Copper Steam Idler"),
            QObject::tr("These required files are missing next to the application:\n%1\n\n"
                        "Reinstall or run the deploy step.")
                .arg(missing.join(QStringLiteral("\n"))));
    }

    MainWindow window;

    if (Settings::instance().noSleep())
        PowerManager::preventSleep();

    if (Settings::instance().startMinimized())
        window.startHidden();
    else
        window.show();

    return app.exec();
}
