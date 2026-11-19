#pragma once
#include <QObject>
#include <QVariantMap>
#include <QTimer>

// Reads Plasma's current desktops; never changes wallpaper configuration.
class Wallpaper : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap outputs READ outputs NOTIFY changed)
public:
    explicit Wallpaper(QObject *parent=nullptr);
    QVariantMap outputs() const { return m_outputs; }
    QString status() const { return m_status; }
    void setEnabled(bool enabled);
signals:
    void changed();
private:
    void refresh();
    QTimer m_timer;
    QVariantMap m_outputs;
    QString m_status;
    bool m_enabled=false,m_pending=false;
    int m_generation=0;
};
