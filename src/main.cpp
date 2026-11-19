#include <QApplication>
#include <QQmlContext>
#include <QLockFile>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPainter>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTabWidget>
#include "Controller.h"
#include "SettingsWindow.h"
#include "Wallpaper.h"
#include <QCommandLineParser>
#include <QQuickView>
#include <QScreen>
#include <QTimer>
#include <QDebug>
#include <LayerShellQt/Window>
#include <memory>
#include <map>

int main(int argc, char **argv)
{
    // This probe deliberately requires native Wayland, never XWayland.
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("kde-synth");
    app.setOrganizationName("kde-synth");
    QCommandLineParser parser;
    parser.setApplicationDescription("Audio spectrum bars for KDE Plasma on Wayland.");
    parser.addHelpOption();
    parser.addOption({"seconds", "Exit automatically after this many seconds (0 = until stopped).", "seconds", "0"});
    parser.addOption({"settings", "Open spectrum settings on launch."});
    parser.addOption({"config", "Use an alternate settings file.", "path"});
    parser.addOption({"snapshot", "Save a settings-window image for layout validation.", "path"});
    parser.process(app);
    bool valid = false;
    const int seconds = parser.value("seconds").toInt(&valid);
    if (!valid || seconds < 0 || seconds > 86400) {
        qCritical() << "--seconds must be between 0 and 86400";
        return 2;
    }
    QLockFile instance(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"/kde-synth.lock");
    instance.setStaleLockTime(0);
    if(!instance.tryLock(0)){
        qWarning()<<"KDE Synth is already running. Use its tray icon for settings or Quit.";
        return 2;
    }
    QString config=parser.value("config");
    if(config.isEmpty())config=QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/kde-synth/settings.ini";
    Controller controller(config);
    Wallpaper wallpaper;
    const auto updateWallpaper=[&]{wallpaper.setEnabled(controller.effect()=="wallpaper-rgb");};
    QObject::connect(&controller,&Controller::settingsChanged,&wallpaper,updateWallpaper);updateWallpaper();
    std::unique_ptr<QWidget> settings(createSettingsWindow(controller,wallpaper));
    const auto makeIcon=[&](double phase){
        QPixmap icon(32,32);icon.fill(Qt::transparent);
        QPainter painter(&icon);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(Qt::NoPen);
        for(int i=0;i<5;++i){
            const bool active=controller.active();
            const double height=active ? 5+18*(.5+.5*std::sin(phase+i*1.7)) : 5;
            painter.setBrush(active?QColor::fromHsv((i*45+190)%360,150,245):QColor("#8c9199"));
            painter.drawRoundedRect(QRectF(2+i*6,28-height,4,height),1,1);
        }
        return QIcon(icon);
    };
    app.setWindowIcon(makeIcon(0));
    QSystemTrayIcon tray{makeIcon(0)};
    QTimer iconTimer;iconTimer.setInterval(100);double iconPhase=0;
    QObject::connect(&iconTimer,&QTimer::timeout,&app,[&]{iconPhase+=.6;tray.setIcon(makeIcon(iconPhase));});
    QMenu menu;
    const auto showSettings=[&]{settings->show();settings->raise();settings->activateWindow();};
    menu.addAction("Settings…",&app,showSettings);
    menu.addSeparator();menu.addAction("Quit",&app,&QCoreApplication::quit);
    tray.setContextMenu(&menu);
    const auto updateTray=[&]{
        tray.setToolTip(!controller.enabled()?"KDE Synth — disabled (click to enable)":controller.active()?"KDE Synth — active (click to disable)":"KDE Synth — waiting for playback (click to disable)");
        if(controller.active())iconTimer.start();else iconTimer.stop();
        tray.setIcon(makeIcon(iconPhase));
    };
    QObject::connect(&controller,&Controller::enabledChanged,&app,updateTray);
    QObject::connect(&controller,&Controller::activeChanged,&app,updateTray);
    QObject::connect(&tray,&QSystemTrayIcon::activated,&app,[&](QSystemTrayIcon::ActivationReason reason){
        if(reason==QSystemTrayIcon::Trigger)controller.setEnabled(!controller.enabled());
    });
    updateTray();tray.show();
    if(parser.isSet("settings")||!QSystemTrayIcon::isSystemTrayAvailable())showSettings();
    QQuickWindow::setDefaultAlphaBuffer(true);
    // Output surfaces are separate from the eventual spectrum layout. QScreen
    // retains logical desktop coordinates, including negative/offset positions.
    std::map<QScreen *, std::unique_ptr<QQuickView>> views;
    const auto addScreen = [&](QScreen *screen) -> bool {
        if (views.count(screen)) return true;
        auto view = std::make_unique<QQuickView>();
        view->setScreen(screen);
        view->setTitle("KDE Synth — spectrum");
        view->setColor(Qt::transparent);
        view->setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->resize(screen->size());
        auto *layer = LayerShellQt::Window::get(view.get());
        layer->setScope("kde-synth");
        layer->setScreen(screen);
        layer->setLayer(LayerShellQt::Window::LayerBottom);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorBottom
                          | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight);
        // Let KWin size the surface inside panel reservations on this output.
        // Zero reserves no space itself, but respects other exclusive surfaces.
        layer->setDesiredSize(QSize(0, 0));
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setActivateOnShow(false);
        view->rootContext()->setContextProperty("visualizer",&controller);
        view->rootContext()->setContextProperty("desktopLayout",controller.desktopLayout());
        view->rootContext()->setContextProperty("outputName",screen->name());
        view->rootContext()->setContextProperty("wallpapers",&wallpaper);
        view->setSource(QUrl("qrc:/src/Bars.qml"));
        if (view->status() == QQuickView::Error)
            return false;
        QObject::connect(view.get(), &QQuickWindow::sceneGraphError, &app,
                         [&app](QQuickWindow::SceneGraphError, const QString &message) {
            qCritical() << "Rendering failed:" << message;
            app.exit(1);
        });
        if(controller.active())view->show();
        QObject::connect(&controller,&Controller::activeChanged,view.get(),[&controller,window=view.get()]{window->setVisible(controller.active());});
        QObject::connect(screen, &QScreen::geometryChanged, view.get(),
                         [screen](const QRect &geometry) {
            qInfo() << "Output layout changed:" << screen->name() << geometry
                    << "scale" << screen->devicePixelRatio();
            // Surface sizing is managed by KWin configure events. Future
            // combined layouts will also recompute their viewports here.
        });
        qInfo() << "Bottom layer requested on" << screen->name() << screen->geometry()
                << "scale" << screen->devicePixelRatio();
        views.emplace(screen, std::move(view));
        return true;
    };
    QObject::connect(&app, &QGuiApplication::screenAdded, &app, [&](QScreen *screen) {
        if (!addScreen(screen)) app.exit(1);
    });
    QObject::connect(&app, &QGuiApplication::screenRemoved, &app, [&](QScreen *screen) {
        // Destroy this output's surface before Qt can migrate it to another
        // monitor, which would create duplicate visualizers there.
        views.erase(screen);
    });
    for (QScreen *screen : app.screens())
        if (!addScreen(screen)) return 1;
    if (views.empty()) return 1;
    if(parser.isSet("snapshot"))QTimer::singleShot(1500,&app,[&]{
        settings->grab().save(parser.value("snapshot"));
        if(auto *tabs=settings->findChild<QTabWidget *>("settingsTabs")){
            const int previous=tabs->currentIndex();
            for(int i=0;i<tabs->count();++i){
                if(!tabs->isTabVisible(i))continue;
                tabs->setCurrentIndex(i);
                settings->grab().save(parser.value("snapshot")+QString(".tab%1.png").arg(i));
            }
            tabs->setCurrentIndex(previous);
        }
        QJsonObject diagnostic{{"enabled",controller.enabled()},{"active",controller.active()},{"automatic",controller.automatic()},{"activity",controller.activityStatus()}};
        QJsonObject regions;
        for(const auto &[screen,view]:views){
            view->grabWindow().save(parser.value("snapshot")+"."+screen->name()+".bars.png");
            const auto rect=controller.desktopLayout()->regions().value(screen->name()).toRectF();
            regions.insert(screen->name(),QJsonObject{{"x",rect.x()},{"y",rect.y()},{"width",rect.width()},{"height",rect.height()}});
        }
        diagnostic.insert("regions",regions);
        diagnostic.insert("slices",QJsonObject::fromVariantMap(controller.desktopLayout()->slices()));
        diagnostic.insert("wallpapers",QJsonObject::fromVariantMap(wallpaper.outputs()));
        diagnostic.insert("visualizerType",controller.visualizerType());
        diagnostic.insert("visualizerOptions",QJsonObject::fromVariantMap(controller.visualizerOptions()));
        diagnostic.insert("waveformPoints",controller.waveform().size());
        diagnostic.insert("effect",controller.effect());
        diagnostic.insert("motionEnabled",controller.motionEnabled());
        diagnostic.insert("motionType",controller.motionType());
        diagnostic.insert("transparency",controller.transparency());
        diagnostic.insert("shakeX",controller.shakeX());
        diagnostic.insert("shakeY",controller.shakeY());
        diagnostic.insert("audioGain",controller.audioGain());
        diagnostic.insert("style",QJsonObject::fromVariantMap(controller.style()));
        diagnostic.insert("bassCutoff",controller.bassCutoff());
        diagnostic.insert("segmented",controller.segmented());
        diagnostic.insert("bassLevel",controller.bassLevel());
        diagnostic.insert("spectrumBands",controller.levels().size());
        QFile file(parser.value("snapshot")+".json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(diagnostic).toJson());
    });
    if (seconds > 0) QTimer::singleShot(seconds * 1000, &app, &QCoreApplication::quit);
    controller.start();
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&controller,&Controller::stop);
    qInfo() << "System output capture. Settings:" << config << "Auto-exit seconds:" << seconds;
    return app.exec();
}
