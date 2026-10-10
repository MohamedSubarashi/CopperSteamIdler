#include "ProcessManager.h"

#include "Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <tlhelp32.h>
#endif

bool ProcessManager::isProcessRunning(const QString& exeName)
{
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare(exeName, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
#else
    Q_UNUSED(exeName);
    return false;
#endif
}

ProcessManager::ProcessManager(QObject* parent)
    : QObject(parent)
{
}

ProcessManager& ProcessManager::instance()
{
    static ProcessManager manager;
    return manager;
}

QString ProcessManager::helperPath() const
{
    const QString name = QStringLiteral("copper-steam-idle.exe");
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

bool ProcessManager::isIdling(int appId) const
{
    const auto it = m_procs.constFind(appId);
    if (it == m_procs.constEnd() || !it.value())
        return false;
    return it.value()->state() != QProcess::NotRunning;
}

void ProcessManager::startIdle(int appId)
{
    if (isIdling(appId))
        return;

    const QString helper = helperPath();
    if (!QFileInfo::exists(helper)) {
        Logger::exception(QStringLiteral("Helper missing"),
                          QStringLiteral("Could not find %1").arg(helper));
        return;
    }

    QProcess* process = new QProcess(this);
    process->setProgram(helper);
    process->setArguments({QString::number(appId)});
    process->setWorkingDirectory(QCoreApplication::applicationDirPath());
    connect(process, &QProcess::finished, this, [this, appId]() { cleanup(appId); });
    connect(process, &QProcess::errorOccurred, this, [this, appId](QProcess::ProcessError) {
        if (m_procs.value(appId) && m_procs.value(appId)->state() == QProcess::NotRunning)
            cleanup(appId);
    });

    m_procs.insert(appId, process);
    process->start();
}

void ProcessManager::stopIdle(int appId)
{
    QProcess* process = m_procs.value(appId, nullptr);
    if (!process)
        return;
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(2000);
    }
    // cleanup happens via the finished signal, but make sure it is removed.
    cleanup(appId);
}

void ProcessManager::cleanup(int appId)
{
    QProcess* process = m_procs.take(appId);
    if (process) {
        process->deleteLater();
    }
}

void ProcessManager::stopAll()
{
    const QList<int> ids = m_procs.keys();
    for (int appId : ids)
        stopIdle(appId);
}

int ProcessManager::idlingCount() const
{
    int count = 0;
    for (auto it = m_procs.constBegin(); it != m_procs.constEnd(); ++it) {
        if (it.value() && it.value()->state() != QProcess::NotRunning)
            ++count;
    }
    return count;
}

QList<int> ProcessManager::idlingAppIds() const
{
    QList<int> ids;
    for (auto it = m_procs.constBegin(); it != m_procs.constEnd(); ++it) {
        if (it.value() && it.value()->state() != QProcess::NotRunning)
            ids.append(it.key());
    }
    return ids;
}

void ProcessManager::killRogueHelpers()
{
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    const DWORD currentPid = GetCurrentProcessId();
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == currentPid)
                continue;
            if (QString::fromWCharArray(entry.szExeFile).compare(
                    QStringLiteral("copper-steam-idle.exe"), Qt::CaseInsensitive) != 0)
                continue;

            bool tracked = false;
            for (auto it = m_procs.constBegin(); it != m_procs.constEnd(); ++it) {
                if (it.value() && static_cast<DWORD>(it.value()->processId()) == entry.th32ProcessID) {
                    tracked = true;
                    break;
                }
            }
            if (tracked)
                continue;

            HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
            if (proc) {
                TerminateProcess(proc, 0);
                CloseHandle(proc);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
#endif
}
