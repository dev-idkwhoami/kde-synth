#include "DesktopLayout.h"
#include "FreeArea.h"
#include "SpectrumLayout.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

static QDBusMessage scriptCall(const QString &method){return QDBusMessage::createMethodCall("org.kde.KWin","/Scripting","org.kde.kwin.Scripting",method);}
DesktopLayout::DesktopLayout(QObject *parent):QObject(parent){
    QDBusConnection::sessionBus().registerObject("/org/kdesynth/Layout",this,QDBusConnection::ExportAllSlots);
    m_watchdog.setInterval(4500);m_watchdog.setSingleShot(true);
    connect(&m_watchdog,&QTimer::timeout,this,[this]{m_regions.clear();m_offsets.clear();m_slices.clear();emit regionsChanged();setStatus("KWin layout unavailable — using normal placement");});
}
DesktopLayout::~DesktopLayout(){unload();QDBusConnection::sessionBus().unregisterObject("/org/kdesynth/Layout");}
void DesktopLayout::setStatus(const QString &value){if(value!=m_status){m_status=value;emit statusChanged(value);}}
void DesktopLayout::unload(){
    if(!m_enabled)return;
    m_enabled=false;++m_generation;m_watchdog.stop();
    auto call=scriptCall("unloadScript");call<<QString("kde-synth-visible-desktop");
    QDBusConnection::sessionBus().call(call,QDBus::Block,1000);
    m_script.close();m_script.remove();m_regions.clear();m_offsets.clear();m_slices.clear();emit regionsChanged();
}
void DesktopLayout::setEnabled(bool enabled){
    if(enabled==m_enabled)return;
    if(!enabled){unload();setStatus("");return;}
    m_enabled=true;const int generation=++m_generation;
    const auto bus=QDBusConnection::sessionBus();
    const auto owner=bus.interface()->serviceOwner("org.kde.KWin");
    if(owner.isValid())m_kwinOwner=owner.value();
    QFile source(":/src/desktop-layout.js");
    if(!source.open(QIODevice::ReadOnly)||!m_script.open()){setStatus("Could not prepare KWin integration");return;}
    QByteArray body=source.readAll();
    const QByteArray endpoint=QJsonDocument(QJsonArray{bus.baseService()}).toJson(QJsonDocument::Compact);
    body.replace("__ENDPOINT__",endpoint.mid(1,endpoint.size()-2));
    body.replace("__PID__",QByteArray::number(QCoreApplication::applicationPid()));
    m_script.write(body);m_script.flush();
    setStatus("Checking visible desktop…");m_watchdog.start();
    // Clear only our own stale script from a previous abnormal exit.
    auto remove=scriptCall("unloadScript");remove<<QString("kde-synth-visible-desktop");
    auto *cleanup=new QDBusPendingCallWatcher(bus.asyncCall(remove,1000),this);
    connect(cleanup,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *done){
        done->deleteLater();if(!m_enabled||generation!=m_generation)return;
        auto load=scriptCall("loadScript");load<<m_script.fileName()<<QString("kde-synth-visible-desktop");
        auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(load,1000),this);
        connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *done){
            QDBusPendingReply<int> reply=*done;done->deleteLater();if(!m_enabled||generation!=m_generation)return;
            if(reply.isError()||reply.value()<0){setStatus("KWin integration unavailable — using normal placement");return;}
            auto run=QDBusMessage::createMethodCall("org.kde.KWin",QString("/Scripting/Script%1").arg(reply.value()),"org.kde.kwin.Script","run");
            auto *runner=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(run,1500),this);
            connect(runner,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *done){
                if(done->isError()&&generation==m_generation)setStatus("KWin script failed — using normal placement");done->deleteLater();
            });
        });
    });
}
void DesktopLayout::Report(const QString &json){
    if(!m_enabled||!calledFromDBus()||message().service()!=m_kwinOwner||json.size()>1024*1024)return;
    const auto document=QJsonDocument::fromJson(json.toUtf8());if(!document.isArray())return;
    QVariantMap regions,offsets;
    QMap<QString,QRectF> globalRegions;
    const auto toRect=[](const QJsonObject &r){return QRectF(r["x"].toDouble(),r["y"].toDouble(),r["width"].toDouble(),r["height"].toDouble());};
    for(const auto &value:document.array()){
        const auto output=value.toObject();const auto area=toRect(output["area"].toObject());QVector<QRectF> obstacles;
        const auto geometry=toRect(output["geometry"].toObject());
        offsets.insert(output["name"].toString(),area.topLeft()-geometry.topLeft());
        for(const auto &window:output["windows"].toArray())obstacles.append(toRect(window.toObject()));
        auto free=largestFreeArea(area,obstacles);
        globalRegions.insert(output["name"].toString(),free);
        if(!free.isEmpty())free.translate(-area.topLeft());
        regions.insert(output["name"].toString(),free);
    }
    const auto slices=spectrumSlices(globalRegions);
    if(m_regions!=regions||m_offsets!=offsets||m_slices!=slices){m_regions=regions;m_offsets=offsets;m_slices=slices;emit regionsChanged();qInfo()<<"Visible desktop regions:"<<regions;}
    m_watchdog.start();setStatus("One spectrum shared across visible desktop areas");
}
