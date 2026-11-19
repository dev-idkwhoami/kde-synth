#include "Controller.h"
#include "Envelope.h"
#include "FreeArea.h"
#include "SpectrumLayout.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
#include <cmath>
#define CHECK(expression) do { if(!(expression)) { std::cerr << "Failed: " << #expression << '\n'; return 1; } } while(false)

class FakeMonitor : public ActivityMonitor {
public:
    void configure(bool enabled,const QString &) override { watching=enabled; }
    bool watching=false;
};
class TestController : public Controller {
public:
    using Controller::Controller;
    int starts=0;
protected:
    void startCapture() override { ++starts; }
    void stopCapture() override {}
};

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    CHECK(waveformDisplaySample(0,true)==0);
    CHECK(std::abs(waveformDisplaySample(.01,true)-.1)<1e-10);
    CHECK(std::abs(waveformDisplaySample(-.01,true)+.1)<1e-10);
    CHECK(waveformDisplaySample(.01,false)==.01);
    // A half-height bar must also reach silence in 300ms, not take a full-height
    // speed assumption or depend on how many analysis packets arrive.
    for(int hz:{30,60,120}){
        Envelope envelope;
        envelope.setTarget(0.5,300);CHECK(envelope.advance(1.0/hz,300)==0.5);
        envelope.setTarget(0,300);
        for(int i=0;i<hz*3/10;++i)envelope.advance(1.0/hz,300);
        CHECK(envelope.value()<1e-9);
    }
    Envelope immediate;immediate.setTarget(1,0);immediate.advance(.016,0);
    immediate.setTarget(0,0);CHECK(immediate.advance(.016,0)==0);
    Envelope step;step.setTarget(1,300);step.advance(.016,300);step.setTarget(.4,300);
    CHECK(std::abs(step.advance(.15,300)-.7)<1e-9);
    CHECK(std::abs(step.advance(.15,300)-.4)<1e-9);
    for(int hz:{30,60,120}){
        BassMotion motion;std::array<double,128> bands{};
        bands[70]=1;motion.update(bands);CHECK(motion.advance(1.0/hz)==0);
        bands[0]=1;motion.update(bands);CHECK(motion.advance(1.0/hz)==1);
        motion.silence();
        for(int i=0;i<static_cast<int>(std::ceil(hz*.18));++i)motion.advance(1.0/hz);
        CHECK(motion.advance(0)<1e-9);
    }
    {
        std::array<double,128> bands{};bands[40]=1;
        BassMotion low,high;
        low.update(bands,150);high.update(bands,300);
        CHECK(low.advance(.016)==0);CHECK(high.advance(.016)==1);
        CHECK(BassMotion::bandCount(150)==33);
        CHECK(BassMotion::bandCount(40)<BassMotion::bandCount(500));
    }
    {
        PeakHold peak;CHECK(peak.update(.8,.016,300,1000)==.8);
        CHECK(peak.update(.2,.2,300,1000)==.8);
        CHECK(std::abs(peak.update(.2,.2,300,1000)-.7)<1e-9);
        CHECK(peak.update(0,1,300,1000)==0);
    }
    QTemporaryDir dir;CHECK(dir.isValid());const auto path=dir.filePath("settings.ini");
    {
        Controller controller(path);controller.setBarWidth(42);controller.setMaxHeight(0);
        CHECK(controller.audioGain()==1);controller.setAudioGain(3.5);
        controller.setStyle("outline",true);controller.setStyle("colorA","#123456");
        CHECK(!controller.motionEnabled());CHECK(controller.levels().size()==128);
        controller.setMotionEnabled(true);CHECK(controller.levels().size()==95);
        controller.setMotionEnabled(false);CHECK(controller.levels().size()==128);
        controller.setMotionEnabled(true);controller.setBassDepth(24);
        controller.setMotionType("shake");CHECK(controller.levels().size()==95);
        controller.setShakeStrength(17);controller.setTransparency(63);
        controller.setSegmented(true);controller.setSegmentHeight(6);controller.setSegmentGap(3);
        controller.setBassCutoff(300);CHECK(controller.levels().size()==128-BassMotion::bandCount(300));
        CHECK(controller.effect()=="rainbow");controller.setEffect("wallpaper-rgb");
        controller.setAnalysisHz(90);controller.setReleaseMs(725);controller.setEnabled(false);controller.save();
    }
    {
        Controller restored(path);
        CHECK(restored.audioGain()==3.5);CHECK(restored.style()["outline"].toBool());CHECK(restored.style()["colorA"]=="#123456");
        restored.setStyle("colorA","invalid");CHECK(restored.style()["colorA"]=="#123456");
        restored.setStyle("glowRadius",100);CHECK(restored.style()["glowRadius"]==30);
        restored.setAudioGain(-1);CHECK(restored.audioGain()==.1);
        restored.setAudioGain(100);CHECK(restored.audioGain()==20);
        restored.setAudioGain(std::nan(""));CHECK(restored.audioGain()==20);
        CHECK(restored.motionEnabled());CHECK(restored.bassDepth()==24);
        CHECK(restored.segmented());CHECK(restored.segmentHeight()==6);CHECK(restored.segmentGap()==3);CHECK(restored.bassCutoff()==300);
        restored.setSegmentHeight(0);CHECK(restored.segmentHeight()==1);
        restored.setSegmentGap(-1);CHECK(restored.segmentGap()==0);
        restored.setBassCutoff(1);CHECK(restored.bassCutoff()==40);
        restored.setBassCutoff(10000);CHECK(restored.bassCutoff()==500);
        CHECK(restored.motionType()=="shake");CHECK(restored.shakeStrength()==17);CHECK(restored.transparency()==63);
        restored.setTransparency(-10);CHECK(restored.transparency()==0);
        restored.setTransparency(999);CHECK(restored.transparency()==100);
        restored.setShakeStrength(99);CHECK(restored.shakeStrength()==40);
        restored.setShakeStrength(-1);CHECK(restored.shakeStrength()==0);
        restored.setMotionType("invalid");CHECK(restored.motionType()=="zoom");
        restored.setMotionEnabled(false);CHECK(restored.levels().size()==128);CHECK(restored.shakeX()==0);CHECK(restored.shakeY()==0);
        restored.setBassDepth(100);CHECK(restored.bassDepth()==40);
        restored.setBassDepth(-1);CHECK(restored.bassDepth()==0);
        CHECK(restored.effect()=="wallpaper-rgb");
        restored.setEffect("unknown");CHECK(restored.effect()=="rainbow");
        CHECK(restored.barWidth()==42);CHECK(restored.maxHeight()==0);
        CHECK(restored.analysisHz()==90);CHECK(restored.releaseMs()==725);CHECK(!restored.enabled());
        restored.setMaxHeight(100);CHECK(restored.maxHeight()==150);
        restored.setBarWidth(0);CHECK(restored.barWidth()==2);
    }
    {
        const auto legacy=dir.filePath("legacy-flow.ini");
        {QSettings settings(legacy,QSettings::IniFormat);settings.setValue("style/flow",true);settings.setValue("appearance/effect","solid");}
        {Controller controller(legacy);CHECK(controller.effect()=="color-flow");controller.save();}
        {QSettings settings(legacy,QSettings::IniFormat);CHECK(!settings.contains("style/flow"));}
        {Controller controller(legacy);CHECK(controller.effect()=="color-flow");controller.setEffect("solid");controller.save();}
        {Controller controller(legacy);CHECK(controller.effect()=="solid");}
    }
    {
        const auto types=dir.filePath("visualizers.ini");
        {Controller c(types);
        CHECK(c.visualizerType()=="bars");c.setReleaseMs(750);
        c.setVisualizerType("curve");c.setVisualizerOption("curveFall",500);CHECK(c.releaseMs()==500);
        c.setVisualizerOption("curveSmooth",99);CHECK(c.visualizerOptions()["curveSmooth"]==10);
        c.setVisualizerType("mirrored");c.setVisualizerOption("mirrorFall",125);CHECK(c.releaseMs()==125);
        c.setVisualizerType("bars");CHECK(c.releaseMs()==750);
        c.setVisualizerOption("waveHistory",999);CHECK(c.visualizerOptions()["waveHistory"]==200);
        c.setVisualizerType("waveform");c.save();}
        {Controller c(types);CHECK(c.visualizerType()=="waveform");CHECK(c.visualizerOptions()["curveFall"]==500);
        CHECK(c.visualizerOptions()["mirrorFall"]==125);CHECK(c.visualizerOptions()["waveHistory"]==200);
        c.setVisualizerType("invalid");CHECK(c.visualizerType()=="waveform");}
    }
    {
        FakeMonitor monitor;
        TestController controller(dir.filePath("source-switch.ini"),nullptr,&monitor);
        controller.start();CHECK(controller.active());CHECK(controller.starts==1);
        controller.setSelectedExecutable("/app/spotify");CHECK(controller.active());CHECK(controller.starts==2);
        controller.setAutomatic(true);CHECK(!controller.active());
        controller.setSelectedExecutable("");CHECK(controller.active());CHECK(controller.starts==3);
        controller.setEnabled(false);controller.setSelectedExecutable("/app/other");CHECK(!controller.active());CHECK(controller.starts==3);
    }
    {
        FakeMonitor monitor;
        TestController controller(dir.filePath("automatic.ini"),nullptr,&monitor);
        controller.setSelectedExecutable("/opt/spotify/spotify");controller.setAutomatic(true);controller.start();
        CHECK(monitor.watching);CHECK(!controller.active());
        monitor.activityChanged(true,"Playing");CHECK(controller.active());CHECK(controller.starts==1);
        monitor.activityChanged(false,"Paused");CHECK(!controller.active());
        monitor.activityChanged(true,"Playing");CHECK(controller.active());
        controller.setEnabled(false);CHECK(!controller.active());CHECK(!monitor.watching);
        monitor.activityChanged(false,"Paused");monitor.activityChanged(true,"Playing");
        CHECK(!controller.active());CHECK(!controller.enabled());
        controller.setEnabled(true);CHECK(controller.active());
        controller.setSelectedExecutable("/usr/bin/other-player");CHECK(!controller.active());
        controller.setAutomatic(false);CHECK(controller.active());
        controller.setEnabled(false);controller.save();
    }
    {
        Controller restored(dir.filePath("automatic.ini"));
        CHECK(!restored.enabled());CHECK(!restored.automatic());
        CHECK(restored.selectedExecutable()=="/usr/bin/other-player");
    }
    {QSettings old(dir.filePath("old-volume.ini"),QSettings::IniFormat);old.setValue("audio/followVolume",false);}
    {Controller migrated(dir.filePath("old-volume.ini"));CHECK(migrated.audioGain()==1);}
    const auto legacyPath=dir.filePath("legacy.ini");
    {QSettings legacy(legacyPath,QSettings::IniFormat);legacy.setValue("motion/bassZoom",true);legacy.setValue("motion/bassDepth",28);}
    {Controller legacy(legacyPath);CHECK(legacy.motionEnabled());CHECK(legacy.motionType()=="zoom");CHECK(legacy.bassDepth()==28);legacy.setMotionEnabled(false);legacy.save();}
    {Controller legacy(legacyPath);CHECK(!legacy.motionEnabled());}
    {
        const auto slices=spectrumSlices({
            {"Z-left",QRectF(-2400,0,600,900)},
            {"A-middle",QRectF(100,0,1200,600)},
            {"B-right",QRectF(4000,0,600,1200)},
            {"covered",QRectF()}});
        CHECK(slices.size()==3);
        CHECK(slices["Z-left"].toMap()["start"].toDouble()==0);
        CHECK(slices["Z-left"].toMap()["end"].toDouble()==.25);
        CHECK(slices["A-middle"].toMap()["start"].toDouble()==.25);
        CHECK(slices["A-middle"].toMap()["end"].toDouble()==.75);
        CHECK(slices["B-right"].toMap()["end"].toDouble()==1);
        CHECK(spectrumSlices({}).isEmpty());
        const auto single=spectrumSlices({{"only",QRectF(-900,-900,300,500)}});
        CHECK(single["only"].toMap()["start"].toDouble()==0);
        CHECK(single["only"].toMap()["end"].toDouble()==1);
        const auto stacked=spectrumSlices({{"lower",QRectF(0,900,100,500)},{"upper",QRectF(0,-900,100,500)}});
        CHECK(stacked["upper"].toMap()["start"].toDouble()==0);
        CHECK(stacked["lower"].toMap()["start"].toDouble()==.5);
    }
    const QRectF monitor(-2560,0,2560,1392);
    CHECK(largestFreeArea(monitor,{} )==monitor);
    CHECK(largestFreeArea(monitor,{monitor}).isEmpty());
    CHECK(largestFreeArea(monitor,{QRectF(-2560,0,1280,1392)})==QRectF(-1280,0,1280,1392));
    const auto hole=largestFreeArea(QRectF(0,0,1000,800),{QRectF(0,0,400,800),QRectF(700,0,300,800)});
    CHECK(hole==QRectF(400,0,300,800));
    const auto streams=QJsonDocument::fromJson(R"([
      {"id":10,"type":"PipeWire:Interface:Client","info":{"props":{"application.process.binary":"spotify"}}},
      {"id":11,"type":"PipeWire:Interface:Node","info":{"state":"running","props":{"client.id":10,"media.class":"Stream/Output/Audio"}}}
    ])").array();
    CHECK(ActivityMonitor::playingFromDump(streams,"/opt/spotify/spotify"));
    CHECK(!ActivityMonitor::playingFromDump(streams,"/opt/brave/brave"));
    auto idle=streams;auto node=idle[1].toObject();auto info=node["info"].toObject();info["state"]="idle";node["info"]=info;idle[1]=node;
    CHECK(!ActivityMonitor::playingFromDump(idle,"/opt/spotify/spotify"));
    std::cout<<"Activation policy, remembered identity, stream selection and visible-area geometry passed.\n";
    std::cout<<"Release timing, immediate decay, setting limits and persistence passed.\n";
}
