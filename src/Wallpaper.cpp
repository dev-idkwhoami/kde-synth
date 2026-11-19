#include "Wallpaper.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QImageReader>
#include <QUrl>

Wallpaper::Wallpaper(QObject *parent):QObject(parent){
    m_timer.setInterval(2000);
    connect(&m_timer,&QTimer::timeout,this,&Wallpaper::refresh);
}
void Wallpaper::setEnabled(bool enabled){
    if(m_enabled==enabled)return;
    m_enabled=enabled;++m_generation;
    if(enabled){m_timer.start();refresh();}
    else {m_timer.stop();m_outputs.clear();m_status.clear();emit changed();}
}
void Wallpaper::refresh(){
    if(!m_enabled||m_pending)return;
    m_pending=true;const int generation=m_generation;
    auto call=QDBusMessage::createMethodCall("org.kde.plasmashell","/PlasmaShell","org.kde.PlasmaShell","evaluateScript");
    // currentConfigGroup selects a read context; readConfig does not mutate it.
    call<<QString::fromLatin1(R"JS(print(JSON.stringify(desktops().map(d => {
        d.currentConfigGroup=["Wallpaper",d.wallpaperPlugin,"General"];
        return {geometry:screenGeometry(d.screen),plugin:d.wallpaperPlugin,
            image:d.readConfig("Image",""),fill:d.readConfig("FillMode",2),
            color:String(d.readConfig("Color","#ffffff")),blur:d.readConfig("Blur",false)};
    }))))JS");
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call,1500),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *done){
        QDBusPendingReply<QString> reply=*done;done->deleteLater();m_pending=false;
        if(!m_enabled||generation!=m_generation)return;
        QVariantMap outputs;
        const auto document=QJsonDocument::fromJson(reply.value().trimmed().toUtf8());
        for(const auto &value:document.array()){
            const auto entry=value.toObject(),g=entry["geometry"].toObject();
            const QRect geometry(g["x"].toInt(),g["y"].toInt(),g["width"].toInt(),g["height"].toInt());
            const QUrl url(entry["image"].toString());
            const QString path=url.isLocalFile()?url.toLocalFile():entry["image"].toString();
            QImageReader reader(path);
            const int fill=entry["fill"].toInt(2);
            // Direct static images first. Package/dynamic/blurred wallpapers need
            // Plasma's media resolver and must not silently render the wrong image.
            if(entry["plugin"]!="org.kde.image"||entry["blur"].toBool()||fill<0||fill>6
                ||!QFileInfo(path).isFile()||!reader.canRead()||reader.supportsAnimation())continue;
            for(auto *screen:QGuiApplication::screens())if(screen->geometry()==geometry){
                outputs.insert(screen->name(),QVariantMap{{"source",QUrl::fromLocalFile(path)},
                    {"fill",fill},{"color",entry["color"].toString()},
                    {"width",geometry.width()},{"height",geometry.height()}});
            }
        }
        const QString status=outputs.size()==QGuiApplication::screens().size()
            ? QString("Lifts dark wallpaper colors; neighboring heights change the hue.")
            : QString("Some wallpapers are unavailable. Using rainbow there; select a static image without blur.");
        if(m_outputs!=outputs||m_status!=status){m_outputs=outputs;m_status=status;emit changed();}
    });
}
