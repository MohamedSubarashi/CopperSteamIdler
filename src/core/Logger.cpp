#include "Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace {
QMutex g_mutex;
}

QString Logger::logFilePath()
{
    return QCoreApplication::applicationDirPath() + "/error.log";
}

void Logger::write(const QString& line)
{
    QMutexLocker lock(&g_mutex);
    QFile file(logFilePath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&file);
    out << line << '\n';
}

void Logger::log(const QString& message)
{
    write(QString("[%1] %2")
              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
                   message));
}

void Logger::error(const QString& message)
{
    write(QString("[%1] ERROR: %2")
              .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
                   message));
}

void Logger::exception(const QString& context, const QString& details)
{
    QString line = QString("[%1] EXCEPTION: %2")
                       .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
                            context);
    if (!details.isEmpty())
        line += " :: " + details;
    write(line);
}
