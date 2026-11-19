#pragma once
#include <QObject>
#include <QSettings>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <array>
#include "Envelope.h"
#include "BassMotion.h"
#include "PeakHold.h"
#include "StyleSettings.h"
#include "VisualizerSettings.h"
#include "ActivityMonitor.h"
#include "DesktopLayout.h"

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString visualizerType READ visualizerType NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap visualizerOptions READ visualizerOptions NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList waveform READ waveform NOTIFY levelsChanged)
    Q_PROPERTY(QVariantMap style READ style NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList peakLevels READ peakLevels NOTIFY levelsChanged)
    Q_PROPERTY(double animationTime READ animationTime NOTIFY levelsChanged)
    Q_PROPERTY(QVariantList levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(bool motionEnabled READ motionEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString motionType READ motionType NOTIFY settingsChanged)
    Q_PROPERTY(int shakeStrength READ shakeStrength NOTIFY settingsChanged)
    Q_PROPERTY(double shakeX READ shakeX NOTIFY levelsChanged)
    Q_PROPERTY(double shakeY READ shakeY NOTIFY levelsChanged)
    Q_PROPERTY(int transparency READ transparency NOTIFY settingsChanged)
    Q_PROPERTY(bool segmented READ segmented NOTIFY settingsChanged)
    Q_PROPERTY(int segmentHeight READ segmentHeight NOTIFY settingsChanged)
    Q_PROPERTY(int segmentGap READ segmentGap NOTIFY settingsChanged)
    Q_PROPERTY(int bassCutoff READ bassCutoff NOTIFY settingsChanged)
    Q_PROPERTY(int bassDepth READ bassDepth NOTIFY settingsChanged)
    Q_PROPERTY(double bassLevel READ bassLevel NOTIFY levelsChanged)
    Q_PROPERTY(QString effect READ effect NOTIFY settingsChanged)
    Q_PROPERTY(bool fitDesktop READ fitDesktop NOTIFY settingsChanged)
    Q_PROPERTY(int barWidth READ barWidth NOTIFY settingsChanged)
    Q_PROPERTY(int maxHeight READ maxHeight NOTIFY settingsChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
public:
    explicit Controller(const QString &configFile, QObject *parent = nullptr, ActivityMonitor *monitor = nullptr);
    ~Controller() override;
    QString visualizerType() const { return m_visualizerType; }
    QVariantMap visualizerOptions() const { return m_visualizerOptions; }
    QVariantList waveform() const {
        if(m_visualizerOptions["waveResponse"]!="boosted")return m_waveform;
        QVariantList result;result.reserve(m_waveform.size());
        for(const auto &sample:m_waveform)result.append(waveformDisplaySample(sample.toDouble(),true));
        return result;
    }
    void setVisualizerType(const QString &type);
    void setVisualizerOption(const QString &key,const QVariant &value);
    QVariantList levels() const { return m_motionEnabled ? m_levels.mid(BassMotion::bandCount(m_bassCutoff)) : m_levels; }
    bool motionEnabled() const { return m_motionEnabled; }
    QString motionType() const { return m_motionType; }
    int shakeStrength() const { return m_shakeStrength; }
    int transparency() const { return m_transparency; }
    double shakeX() const;
    double shakeY() const;
    void setMotionType(const QString &type);
    void setShakeStrength(int pixels);
    void setTransparency(int percent);
    bool segmented() const { return m_segmented; }
    int segmentHeight() const { return m_segmentHeight; }
    int segmentGap() const { return m_segmentGap; }
    int bassCutoff() const { return m_bassCutoff; }
    void setSegmented(bool enabled);
    void setSegmentHeight(int pixels);
    void setSegmentGap(int pixels);
    void setBassCutoff(int hz);
    int bassDepth() const { return m_bassDepth; }
    double bassLevel() const { return m_motionEnabled ? m_bassLevel : 0; }
    void setMotionEnabled(bool enabled);
    void setBassDepth(int percent);
    QString effect() const { return m_effect; }
    void setEffect(const QString &effect);
    int barWidth() const { return m_width; }
    int maxHeight() const { return m_height; }
    double audioGain() const { return m_audioGain; }
    void setAudioGain(double gain);
    QVariantMap style() const { return m_style; }
    void setStyle(const QString &key,const QVariant &value);
    QVariantList peakLevels() const { return m_motionEnabled ? m_peakLevels.mid(BassMotion::bandCount(m_bassCutoff)) : m_peakLevels; }
    double animationTime() const { return m_animationTime; }
    int analysisHz() const { return m_hz; }
    int barReleaseMs() const { return m_release; }
    int releaseMs() const { return m_visualizerType=="curve"?m_visualizerOptions["curveFall"].toInt():m_visualizerType=="mirrored"?m_visualizerOptions["mirrorFall"].toInt():m_release; }
    bool enabled() const { return m_enabled; }
    bool active() const { return m_active; }
    bool automatic() const { return m_auto; }
    bool fitDesktop() const { return m_fit; }
    QString selectedExecutable() const { return m_executable; }
    QString activityStatus() const { return m_activityStatus; }
    DesktopLayout *desktopLayout() { return &m_layout; }
    QString status() const { return m_status; }
    void setBarWidth(int value);
    void setMaxHeight(int value);
    void setAnalysisHz(int value);
    void setReleaseMs(int value);
    void setEnabled(bool value);
    void setAutomatic(bool value);
    void setSelectedExecutable(const QString &value);
    void setFitDesktop(bool value);
    void start();
    void stop();
    void save();
signals:
    void levelsChanged();
    void settingsChanged();
    void enabledChanged();
    void activeChanged();
    void activityStatusChanged(const QString &text);
    void statusChanged(const QString &text);
protected:
    virtual void startCapture();
    virtual void stopCapture();
private:
    void readFrames();
    void tick();
    void setStatus(const QString &text);
    void reconcile();
    void configureMonitor();
    QSettings m_settings;
    QProcess m_audio;
    QTimer m_timer, m_saveTimer;
    QElapsedTimer m_clock, m_lastFrame;
    QByteArray m_pending;
    QVariantList m_levels,m_waveform;
    bool m_waveformDirty=false;
    QString m_visualizerType="bars";
    QVariantMap m_visualizerOptions=defaultVisualizers();
    std::array<Envelope,128> m_envelopes{};
    int m_width = 24, m_height = 400, m_hz = 60, m_release = 300;
    bool m_segmented=false;
    int m_segmentHeight=6,m_segmentGap=3,m_bassCutoff=150;
    std::array<double,128> m_rawBands{};
    BassMotion m_bassMotion;
    double m_bassLevel=0;
    bool m_motionEnabled=false;
    int m_bassDepth=15,m_shakeStrength=8,m_transparency=0;
    QString m_motionType="zoom";
    double m_motionTime=0;
    double m_audioGain=1,m_animationTime=0;
    QVariantMap m_style=defaultStyle();
    QVariantList m_peakLevels;
    std::array<PeakHold,128> m_peaks{};
    bool m_enabled = true;
    bool m_active=false,m_auto=false,m_fit=false,m_sourcePlaying=false,m_started=false;
    QString m_effect="rainbow";
    QString m_executable,m_activityStatus="Select an application";
    ActivityMonitor *m_monitor;
    DesktopLayout m_layout;
    QString m_status = "Starting audio capture…";
};
