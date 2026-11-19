#include "Controller.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>
#include <cmath>

Controller::Controller(const QString &path, QObject *parent, ActivityMonitor *monitor)
    : QObject(parent), m_settings(path, QSettings::IniFormat) {
    m_monitor=monitor?monitor:new ActivityMonitor(this);
    m_width = std::clamp(m_settings.value("bars/width",24).toInt(),2,160);
    m_height = m_settings.value("bars/maxHeight",400).toInt();
    if (m_height != 0) m_height = std::clamp(m_height,150,16384);
    m_audioGain=m_settings.value("audio/gain",1.0).toDouble();
    if(!std::isfinite(m_audioGain))m_audioGain=1;
    m_audioGain=std::clamp(m_audioGain,.1,20.0);
    for(auto it=m_style.begin();it!=m_style.end();++it){
        const auto value=validatedStyle(it.key(),m_settings.value("style/"+it.key(),it.value()));
        if(value.isValid())it.value()=value;
    }
    m_visualizerType=m_settings.value("visualizer/type","bars").toString();
    if(!QStringList{"bars","curve","waveform","mirrored"}.contains(m_visualizerType))m_visualizerType="bars";
    for(auto it=m_visualizerOptions.begin();it!=m_visualizerOptions.end();++it){
        const auto value=validateVisualizer(it.key(),m_settings.value("visualizer/"+it.key(),it.value()));
        if(value.isValid())it.value()=value;
    }
    m_hz = std::clamp(m_settings.value("audio/analysisHz",60).toInt(),10,120);
    m_release = std::clamp(m_settings.value("bars/releaseMs",300).toInt(),0,5000);
    m_motionEnabled=m_settings.value("motion/enabled",m_settings.value("motion/bassZoom",false)).toBool();
    m_motionType=m_settings.value("motion/type","zoom").toString();
    if(m_motionType!="shake")m_motionType="zoom";
    m_shakeStrength=std::clamp(m_settings.value("motion/shakeStrength",8).toInt(),0,40);
    m_transparency=std::clamp(m_settings.value("appearance/transparency",0).toInt(),0,100);
    m_segmented=m_settings.value("appearance/segmented",false).toBool();
    m_segmentHeight=std::clamp(m_settings.value("appearance/segmentHeight",6).toInt(),1,100);
    m_segmentGap=std::clamp(m_settings.value("appearance/segmentGap",3).toInt(),0,50);
    m_bassCutoff=std::clamp(m_settings.value("motion/bassCutoff",150).toInt(),40,500);
    m_bassDepth=std::clamp(m_settings.value("motion/bassDepth",15).toInt(),0,40);
    m_enabled = m_settings.value("enabled",true).toBool();
    m_auto=m_settings.value("automatic/enabled",false).toBool();
    m_executable=m_settings.value("automatic/executable").toString();
    m_effect=m_settings.value("appearance/effect","rainbow").toString();
    if(!QStringList{"rainbow","wallpaper-rgb","solid","gradient","intensity","color-flow"}.contains(m_effect))m_effect="rainbow";
    if(m_settings.value("style/flow",false).toBool())m_effect="color-flow";
    m_fit=m_settings.value("bars/fitDesktop",false).toBool();
    connect(m_monitor,&ActivityMonitor::activityChanged,this,[this](bool playing,const QString &text){
        m_sourcePlaying=playing;m_activityStatus=text;emit activityStatusChanged(text);reconcile();
    });
    for (int i=0;i<128;++i) {m_levels.append(0.0);m_peakLevels.append(0.0);}
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer,&QTimer::timeout,this,&Controller::save);
    connect(this,&Controller::settingsChanged,&m_saveTimer,qOverload<>(&QTimer::start));
    connect(this,&Controller::enabledChanged,&m_saveTimer,qOverload<>(&QTimer::start));
    connect(&m_audio,&QProcess::readyReadStandardOutput,this,&Controller::readFrames);
    connect(&m_audio,&QProcess::readyReadStandardError,this,[this]{
        const QString error = QString::fromUtf8(m_audio.readAllStandardError()).trimmed();
        if (!error.isEmpty()) { qWarning().noquote()<<error; setStatus("Audio: " + error.left(180)); }
    });
    connect(&m_audio,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){
        setStatus("Audio helper: " + m_audio.errorString());
    });
    connect(&m_audio,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        if (m_active) setStatus(QString("Audio capture stopped (%1). Toggle off/on to retry.").arg(code));
    });
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer,&QTimer::timeout,this,&Controller::tick);
}
Controller::~Controller(){stop();save();}
void Controller::setStatus(const QString &text){if (m_status!=text){m_status=text;emit statusChanged(text);}}
void Controller::save(){
    m_settings.setValue("visualizer/type",m_visualizerType);
    for(auto it=m_visualizerOptions.cbegin();it!=m_visualizerOptions.cend();++it)m_settings.setValue("visualizer/"+it.key(),it.value());
    m_settings.setValue("motion/enabled",m_motionEnabled);
    m_settings.remove("motion/bassZoom");
    m_settings.setValue("motion/type",m_motionType);
    m_settings.setValue("motion/shakeStrength",m_shakeStrength);
    m_settings.setValue("appearance/transparency",m_transparency);
    m_settings.setValue("appearance/segmented",m_segmented);
    m_settings.setValue("appearance/segmentHeight",m_segmentHeight);
    m_settings.setValue("appearance/segmentGap",m_segmentGap);
    m_settings.setValue("motion/bassCutoff",m_bassCutoff);
    m_settings.setValue("motion/bassDepth",m_bassDepth);
    m_settings.setValue("appearance/effect",m_effect);
    m_settings.setValue("bars/width",m_width);
    m_settings.setValue("bars/maxHeight",m_height);
    m_settings.setValue("bars/releaseMs",m_release);
    m_settings.remove("audio/followVolume");
    m_settings.remove("style/flow");
    m_settings.setValue("audio/gain",m_audioGain);
    for(auto it=m_style.cbegin();it!=m_style.cend();++it)m_settings.setValue("style/"+it.key(),it.value());
    m_settings.setValue("audio/analysisHz",m_hz);
    m_settings.setValue("enabled",m_enabled);
    m_settings.setValue("automatic/enabled",m_auto);
    m_settings.setValue("automatic/executable",m_executable);
    m_settings.setValue("bars/fitDesktop",m_fit);
    m_settings.sync();
    if (m_settings.status()!=QSettings::NoError) setStatus("Could not save settings: " + m_settings.fileName());
}
void Controller::setVisualizerType(const QString &type){
    if(!QStringList{"bars","curve","waveform","mirrored"}.contains(type)||type==m_visualizerType)return;
    m_visualizerType=type;
    for(auto &envelope:m_envelopes)envelope.setRelease(releaseMs());
    emit settingsChanged();emit levelsChanged();
}
void Controller::setVisualizerOption(const QString &key,const QVariant &value){
    const auto valid=validateVisualizer(key,value);
    if(!valid.isValid()||m_visualizerOptions.value(key)==valid)return;
    m_visualizerOptions[key]=valid;
    if(key=="curveFall"||key=="mirrorFall")for(auto &envelope:m_envelopes)envelope.setRelease(releaseMs());
    if(key=="waveHistory"&&m_audio.state()==QProcess::Running)
        m_audio.write(QJsonDocument(QJsonObject{{"waveMs",valid.toInt()}}).toJson(QJsonDocument::Compact)+"\n");
    emit settingsChanged();emit levelsChanged();
}
void Controller::setEffect(const QString &value){
    const QString next=QStringList{"rainbow","wallpaper-rgb","solid","gradient","intensity","color-flow"}.contains(value)?value:QString("rainbow");
    if(next==m_effect)return;
    m_effect=next;m_layout.setEnabled(m_started&&(m_fit||m_effect=="wallpaper-rgb"));emit settingsChanged();
}
void Controller::setMotionEnabled(bool enabled){
    if(m_motionEnabled==enabled)return;
    m_motionEnabled=enabled;m_motionTime=0;emit settingsChanged();emit levelsChanged();
}
void Controller::setMotionType(const QString &type){
    const QString next=type=="shake"?type:QString("zoom");
    if(next==m_motionType)return;
    m_motionType=next;m_motionTime=0;emit settingsChanged();emit levelsChanged();
}
void Controller::setShakeStrength(int pixels){
    m_shakeStrength=std::clamp(pixels,0,40);emit settingsChanged();emit levelsChanged();
}
void Controller::setTransparency(int percent){
    m_transparency=std::clamp(percent,0,100);emit settingsChanged();
}
double Controller::shakeX() const {
    return m_motionEnabled&&m_motionType=="shake" ? std::sin(m_motionTime*56.54866776)*m_bassLevel*m_shakeStrength : 0;
}
double Controller::shakeY() const {
    return m_motionEnabled&&m_motionType=="shake" ? std::sin(m_motionTime*81.68140899)*m_bassLevel*m_shakeStrength*.6 : 0;
}
void Controller::setSegmented(bool enabled){m_segmented=enabled;emit settingsChanged();}
void Controller::setSegmentHeight(int pixels){m_segmentHeight=std::clamp(pixels,1,100);emit settingsChanged();}
void Controller::setSegmentGap(int pixels){m_segmentGap=std::clamp(pixels,0,50);emit settingsChanged();}
void Controller::setBassCutoff(int hz){
    m_bassCutoff=std::clamp(hz,40,500);
    m_bassMotion=BassMotion{};m_bassLevel=0;m_motionTime=0;
    m_bassMotion.update(m_rawBands,m_bassCutoff);
    emit settingsChanged();emit levelsChanged();
}
void Controller::setBassDepth(int percent){
    m_bassDepth=std::clamp(percent,0,40);emit settingsChanged();
}
void Controller::setBarWidth(int v){m_width=std::clamp(v,2,160);emit settingsChanged();}
void Controller::setMaxHeight(int v){m_height=v==0?0:std::clamp(v,150,16384);emit settingsChanged();}
void Controller::setStyle(const QString &key,const QVariant &value){
    const auto valid=validatedStyle(key,value);if(!valid.isValid()||m_style.value(key)==valid)return;
    m_style[key]=valid;
    if(key=="peaks"){m_peaks.fill(PeakHold{});for(auto &p:m_peakLevels)p=0.0;emit levelsChanged();}
    emit settingsChanged();
}
void Controller::setAudioGain(double gain){
    if(!std::isfinite(gain))return;
    m_audioGain=std::clamp(gain,.1,20.0);
    if(m_audio.state()==QProcess::Running)
        m_audio.write(QJsonDocument(QJsonObject{{"gain",m_audioGain}}).toJson(QJsonDocument::Compact)+"\n");
    emit settingsChanged();
}
void Controller::setAnalysisHz(int v){
    m_hz=std::clamp(v,10,120);
    if (m_audio.state()==QProcess::Running)
        m_audio.write(QJsonDocument(QJsonObject{{"hz",m_hz}}).toJson(QJsonDocument::Compact)+"\n");
    emit settingsChanged();
}
void Controller::setReleaseMs(int v){
    m_release=std::clamp(v,0,5000);
    for (auto &envelope:m_envelopes) envelope.setRelease(m_release);
    emit settingsChanged();
}
void Controller::setEnabled(bool v){
    if(m_enabled==v)return;
    m_enabled=v;
    configureMonitor();reconcile();
    emit enabledChanged();
}
void Controller::configureMonitor(){m_monitor->configure(m_started&&m_enabled&&m_auto,m_executable);}
void Controller::setAutomatic(bool value){
    if(m_auto==value)return;
    m_auto=value;m_sourcePlaying=false;configureMonitor();reconcile();emit settingsChanged();
}
void Controller::setSelectedExecutable(const QString &value){
    if(m_executable==value)return;
    const bool wasActive=m_active;
    if(wasActive)stopCapture();
    m_executable=value;m_sourcePlaying=false;configureMonitor();reconcile();
    if(wasActive&&m_active)startCapture();emit settingsChanged();
}
void Controller::setFitDesktop(bool value){
    if(m_fit==value)return;
    m_fit=value;m_layout.setEnabled(m_started&&(value||m_effect=="wallpaper-rgb"));emit settingsChanged();
}
void Controller::reconcile(){
    const bool next=m_started&&m_enabled&&(!m_auto||m_executable.isEmpty()||m_sourcePlaying);
    if(next!=m_active){m_active=next;if(next)startCapture();else stopCapture();emit activeChanged();}
    if(!m_enabled)setStatus("Disabled");
    else if(m_auto&&!m_executable.isEmpty()&&!m_sourcePlaying)setStatus("Waiting for selected application");
}
void Controller::start(){m_started=true;configureMonitor();m_layout.setEnabled(m_fit||m_effect=="wallpaper-rgb");reconcile();}
void Controller::stop(){m_started=false;configureMonitor();reconcile();stopCapture();m_layout.setEnabled(false);}
void Controller::startCapture(){
    if(m_audio.state()!=QProcess::NotRunning)return;
    m_pending.clear();m_lastFrame.invalidate();m_clock.start();
    const QString localHelper = QCoreApplication::applicationDirPath()+"/kde-synth-audio";
    m_audio.setProgram(QFileInfo(localHelper).isExecutable() ? localHelper : QStringLiteral(KDE_SYNTH_AUDIO_PATH));
    m_audio.setArguments({"--hz",QString::number(m_hz),"--gain",QString::number(m_audioGain),"--wave-ms",QString::number(m_visualizerOptions["waveHistory"].toInt()),"--app",m_executable});
    m_audio.start();m_timer.start();setStatus(m_executable.isEmpty()?"Connecting to desktop audio…":"Waiting for selected application audio…");
}
void Controller::stopCapture(){
    m_timer.stop();m_audio.closeWriteChannel();
    if(m_audio.state()!=QProcess::NotRunning && !m_audio.waitForFinished(1500)) {
        m_audio.terminate();
        if(!m_audio.waitForFinished(1000)){m_audio.kill();m_audio.waitForFinished(1000);}
    }
    m_waveform.clear();
    m_envelopes.fill(Envelope{});m_peaks.fill(PeakHold{});
    for(auto &p:m_peakLevels)p=0.0;
    m_animationTime=0;
    m_bassMotion=BassMotion{};m_bassLevel=0;m_motionTime=0;m_rawBands.fill(0);
    for(auto &level:m_levels)level=0.0;
    emit levelsChanged();
}
void Controller::readFrames(){
    m_pending+=m_audio.readAllStandardOutput();
    while(true){
        const auto end=m_pending.indexOf('\n');if(end<0)break;
        const auto object=QJsonDocument::fromJson(m_pending.left(end)).object();m_pending.remove(0,end+1);
        const auto bands=object.value("bands").toArray();if(bands.size()!=128)continue;
        std::array<double,128> clean{};
        for(int i=0;i<128;++i){
            double next=bands[i].toDouble();if(!std::isfinite(next))next=0;
            next=std::clamp(next,0.0,1.0);
            clean[i]=next;
            m_envelopes[i].setTarget(next,releaseMs());
        }
        m_waveform.clear();
        const auto wave=object.value("waveform").toArray();
        if(wave.size()==512)for(const auto &sample:wave){
            const double v=sample.toDouble();
            m_waveform.append(std::isfinite(v)?std::clamp(v,-1.0,1.0):0.0);
        }
        m_waveformDirty=true;
        m_rawBands=clean;m_bassMotion.update(clean,m_bassCutoff);
        m_lastFrame.restart();
        setStatus(object.value("capturing").toBool() ? (m_executable.isEmpty()?"Listening to desktop audio":"Listening to selected application audio") : (m_executable.isEmpty()?"Waiting for desktop audio…":"Waiting for selected application audio…"));
    }
    if(m_pending.size()>1048576)m_pending.clear();
}
void Controller::tick(){
    const double dt=std::min(0.25,m_clock.restart()/1000.0);
    if(m_lastFrame.isValid()&&m_lastFrame.elapsed()>350){
        for(auto &envelope:m_envelopes) envelope.setTarget(0,releaseMs());
        m_rawBands.fill(0);m_bassMotion.silence();
        if(!m_waveform.isEmpty()){m_waveform.clear();emit levelsChanged();}
    }
    const double bass=m_bassMotion.advance(dt);
    bool changed=std::abs(bass-m_bassLevel)>0.00001 || (m_waveformDirty&&m_visualizerType=="waveform");
    m_waveformDirty=false;
    const bool shaking=m_motionEnabled&&m_motionType=="shake"&&m_shakeStrength>0;
    if(shaking&&(bass>0.00001||m_bassLevel>0.00001))changed=true;
    m_motionTime=bass>0.00001?std::fmod(m_motionTime+dt,1000.0):0;
    m_bassLevel=bass;
    for(int i=0;i<128;++i){
        const double next=m_envelopes[i].advance(dt,releaseMs());
        if(std::abs(next-m_levels[i].toDouble())>0.00001){changed=true;m_levels[i]=next;}
    }
    if(m_style["peaks"].toBool()){
        for(int i=0;i<128;++i){
            const double p=m_peaks[i].update(m_levels[i].toDouble(),dt,m_style["peakHold"].toInt(),m_style["peakFall"].toInt());
            if(std::abs(p-m_peakLevels[i].toDouble())>0.00001){m_peakLevels[i]=p;changed=true;}
        }
    }
    if(m_effect=="color-flow"&&m_style["flowSpeed"].toInt()!=0){m_animationTime=std::fmod(m_animationTime+dt*m_style["flowSpeed"].toInt()/100.0,1.0);changed=true;}
    if(changed)emit levelsChanged();
}
