#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QJsonArray>
#include <QStringList>

class ActivityMonitor : public QObject {
    Q_OBJECT
public:
    explicit ActivityMonitor(QObject *parent=nullptr);
    ~ActivityMonitor() override;
    virtual void configure(bool enabled,const QString &executable);
    static QStringList runningExecutables();
    static QString executableForPid(qint64 pid);
    static bool playingFromDump(const QJsonArray &objects,const QString &executable);
signals:
    void activityChanged(bool playing,const QString &description);
private:
    void poll();
    void evaluate(const QJsonArray &objects);
    QProcess m_dump;
    QTimer m_timer,m_timeout;
    QString m_executable;
    bool m_enabled=false;
    int m_generation=0;
};
