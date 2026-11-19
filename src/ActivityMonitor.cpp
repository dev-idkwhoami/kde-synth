#include "ActivityMonitor.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QSet>
#include <memory>
#include <unistd.h>

ActivityMonitor::ActivityMonitor(QObject *parent):QObject(parent){
    m_timer.setInterval(750);
    m_timeout.setSingleShot(true);m_timeout.setInterval(2000);
    connect(&m_timer,&QTimer::timeout,this,&ActivityMonitor::poll);
    connect(&m_timeout,&QTimer::timeout,this,[this]{m_dump.kill();});
    connect(&m_dump,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){
        if(m_enabled)emit activityChanged(false,"Audio activity unavailable");
    });
    connect(&m_dump,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        m_timeout.stop();
        const auto bytes=m_dump.readAllStandardOutput();m_dump.readAllStandardError();
        if(!m_enabled)return;
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
        if(code!=0||error.error!=QJsonParseError::NoError||!doc.isArray()){
            emit activityChanged(false,"Audio activity unavailable");return;
        }
        evaluate(doc.array());
    });
}
ActivityMonitor::~ActivityMonitor(){m_dump.kill();m_dump.waitForFinished(1000);}
QString ActivityMonitor::executableForPid(qint64 pid){
    if(pid<=0)return {};
    QString path=QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget();
    if(path.endsWith(" (deleted)"))path.chop(10);
    return path;
}
QStringList ActivityMonitor::runningExecutables(){
    QSet<QString> paths;
    for(const auto &name:QDir("/proc").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
        bool valid=false;const auto pid=name.toLongLong(&valid);
        if(!valid||QFileInfo("/proc/"+name).ownerId()!=getuid())continue;
        const auto path=executableForPid(pid);
        if(!path.isEmpty()&&!path.contains("/kde-synth"))paths.insert(path);
    }
    QStringList result(paths.begin(),paths.end());
    std::sort(result.begin(),result.end(),[](const QString&a,const QString&b){
        const int names=QString::compare(QFileInfo(a).fileName(),QFileInfo(b).fileName(),Qt::CaseInsensitive);
        return names==0?a<b:names<0;
    });return result;
}
bool ActivityMonitor::playingFromDump(const QJsonArray &objects,const QString &executable){
    if(executable.isEmpty())return false;
    QHash<int,QJsonObject> clients;
    for(const auto &value:objects){const auto o=value.toObject();if(o["type"].toString()=="PipeWire:Interface:Client")clients[o["id"].toInt()]=o["info"].toObject()["props"].toObject();}
    for(const auto &value:objects){
        const auto object=value.toObject(),info=object["info"].toObject();
        auto props=info["props"].toObject();
        if(object["type"].toString()!="PipeWire:Interface:Node"||props["media.class"].toString()!="Stream/Output/Audio"||info["state"].toString()!="running")continue;
        const auto client=clients.value(props["client.id"].toInt());
        for(auto it=client.begin();it!=client.end();++it)if(!props.contains(it.key()))props.insert(it.key(),it.value());
        const auto pid=props["application.process.id"].toVariant().toLongLong();
        const auto path=executableForPid(pid);
        if(!path.isEmpty()){if(path==executable)return true;}
        else if(props["application.process.binary"].toString()==QFileInfo(executable).fileName())return true;
    }
    return false;
}
void ActivityMonitor::configure(bool enabled,const QString &executable){
    if(m_enabled==enabled&&m_executable==executable)return;
    ++m_generation;m_enabled=enabled;m_executable=executable;
    m_timer.stop();m_timeout.stop();
    // A result from the previous selection must not activate the new selection.
    if(m_dump.state()!=QProcess::NotRunning){m_dump.kill();m_dump.waitForFinished(500);}
    emit activityChanged(false,executable.isEmpty()?"Select an application":"Waiting for playback");
    if(enabled&&!executable.isEmpty()){m_timer.start();poll();}
}
void ActivityMonitor::poll(){
    if(!m_enabled||m_executable.isEmpty()||m_dump.state()!=QProcess::NotRunning)return;
    m_dump.start("pw-dump",{});m_timeout.start();
}
void ActivityMonitor::evaluate(const QJsonArray &objects){
    const bool streamPlaying=playingFromDump(objects,m_executable);
    // Prefer a selected player's explicit playback state when it exposes MPRIS.
    // Generic applications fall back to their PipeWire playback stream state.
    const auto bus=QDBusConnection::sessionBus();
    const auto services=bus.interface()->registeredServiceNames();
    QStringList matches;
    if(services.isValid())for(const auto &service:services.value()){
        if(!service.startsWith("org.mpris.MediaPlayer2."))continue;
        const auto pid=bus.interface()->servicePid(service);
        if(pid.isValid()&&executableForPid(pid.value())==m_executable)matches.append(service);
    }
    if(matches.isEmpty()){emit activityChanged(streamPlaying,streamPlaying?"Playing":"Waiting for playback");return;}
    struct Results{int remaining=0;bool valid=false,playing=false;};
    auto result=std::make_shared<Results>();result->remaining=matches.size();
    const int generation=m_generation;
    for(const auto &service:matches){
        auto call=QDBusMessage::createMethodCall(service,"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","Get");
        call<<QString("org.mpris.MediaPlayer2.Player")<<QString("PlaybackStatus");
        auto *watcher=new QDBusPendingCallWatcher(bus.asyncCall(call,250),this);
        connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,result,generation,streamPlaying](QDBusPendingCallWatcher *watcher){
            QDBusPendingReply<QDBusVariant> reply=*watcher;watcher->deleteLater();
            if(generation!=m_generation||!m_enabled)return;
            if(!reply.isError()){result->valid=true;result->playing|=reply.value().variant().toString()=="Playing";}
            if(--result->remaining==0){const bool playing=result->valid?result->playing:streamPlaying;emit activityChanged(playing,playing?"Playing":"Waiting for playback");}
        });
    }
}
