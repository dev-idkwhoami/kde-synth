#pragma once
#include <QObject>
#include <QVariantMap>
#include <QTemporaryFile>
#include <QTimer>
#include <QDBusContext>

class DesktopLayout : public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.kdesynth.Layout")
    Q_PROPERTY(QVariantMap slices READ slices NOTIFY regionsChanged)
    Q_PROPERTY(QVariantMap offsets READ offsets NOTIFY regionsChanged)
    Q_PROPERTY(QVariantMap regions READ regions NOTIFY regionsChanged)
public:
    explicit DesktopLayout(QObject *parent=nullptr);
    ~DesktopLayout() override;
    QVariantMap slices()const{return m_slices;}
    QVariantMap offsets()const{return m_offsets;}
    QVariantMap regions()const{return m_regions;}
    void setEnabled(bool enabled);
    bool enabled()const{return m_enabled;}
    QString status()const{return m_status;}
public slots:
    void Report(const QString &json);
signals:
    void regionsChanged();
    void statusChanged(const QString &status);
private:
    void setStatus(const QString &status);
    void unload();
    bool m_enabled=false;
    int m_generation=0;
    QTemporaryFile m_script;
    QTimer m_watchdog;
    QVariantMap m_regions,m_offsets,m_slices;
    QString m_status,m_kwinOwner;
};
