#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QProcess;

// Starts/stops one copper-steam-idle helper process per idled AppID.
class ProcessManager : public QObject
{
    Q_OBJECT
public:
    static ProcessManager& instance();

    bool isIdling(int appId) const;
    void startIdle(int appId);
    void stopIdle(int appId);
    void stopAll();

    int idlingCount() const;
    QList<int> idlingAppIds() const;

    // Terminates helper processes not tracked by this manager (leftovers).
    void killRogueHelpers();

    // Case-insensitive check for a running process by executable name.
    static bool isProcessRunning(const QString& exeName);

    QString helperPath() const;

private:
    explicit ProcessManager(QObject* parent = nullptr);
    void cleanup(int appId);

    QHash<int, QProcess*> m_procs;
};
