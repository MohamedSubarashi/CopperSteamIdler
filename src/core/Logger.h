#pragma once

#include <QString>

// Appends diagnostics next to the executable (error.log), mirroring the
// original Logger behaviour. Thread-safe.
class Logger
{
public:
    static void log(const QString& message);
    static void error(const QString& message);
    static void exception(const QString& context, const QString& details = QString());
    static QString logFilePath();

private:
    static void write(const QString& line);
};
