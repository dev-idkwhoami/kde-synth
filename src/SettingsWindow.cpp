#include "SettingsWindow.h"
#include "Controller.h"
#include "Wallpaper.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileInfo>
#include <QPushButton>
#include <QScreen>
#include <QApplication>
#include <QDialogButtonBox>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QScrollArea>

QWidget *createSettingsWindow(Controller &controller, Wallpaper &wallpaper){
    auto *window=new QWidget;
    window->setWindowTitle("KDE Synthesizer");window->setMinimumWidth(640);window->resize(680,700);
    auto *layout=new QVBoxLayout(window);layout->setContentsMargins(24,24,24,24);layout->setSpacing(14);
    auto *heading=new QLabel("KDE Synthesizer");auto font=heading->font();font.setPointSize(font.pointSize()+5);font.setBold(true);heading->setFont(font);layout->addWidget(heading);
    auto *enabled=new QCheckBox("Enable visualizer");enabled->setChecked(controller.enabled());layout->addWidget(enabled);
    QObject::connect(enabled,&QCheckBox::toggled,&controller,&Controller::setEnabled);
    QObject::connect(&controller,&Controller::enabledChanged,window,[&controller,enabled]{enabled->setChecked(controller.enabled());});
    auto *type=new QComboBox;
    type->addItem("Spectrum bars","bars");type->addItem("Spectrum curve","curve");
    type->addItem("Waveform","waveform");type->addItem("Mirrored spectrum","mirrored");
    type->setCurrentIndex(type->findData(controller.visualizerType()));
    auto *typeRow=new QFormLayout;typeRow->addRow("Visualizer",type);layout->addLayout(typeRow);
    QObject::connect(type,qOverload<int>(&QComboBox::currentIndexChanged),window,[type,&controller](int i){controller.setVisualizerType(type->itemData(i).toString());});
    auto *tabs=new QTabWidget;tabs->setObjectName("settingsTabs");layout->addWidget(tabs,1);
    const auto addPage=[tabs](const QString &title){
        auto *page=new QWidget;auto *pageLayout=new QVBoxLayout(page);
        pageLayout->setContentsMargins(18,20,18,20);
        auto *form=new QFormLayout;form->setSpacing(10);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        pageLayout->addLayout(form);pageLayout->addStretch();
        auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);tabs->addTab(scroll,title);return form;
    };
    auto *form=addPage("Appearance");
    auto hint=[&form](const QString &text){
        auto *label=new QLabel(text);label->setWordWrap(true);auto font=label->font();font.setPointSize(std::max(8,font.pointSize()-1));label->setFont(font);
        auto palette=label->palette();const auto fg=palette.color(QPalette::WindowText),bg=palette.color(QPalette::Window);
        palette.setColor(QPalette::WindowText,QColor::fromRgbF(fg.redF()*.65+bg.redF()*.35,fg.greenF()*.65+bg.greenF()*.35,fg.blueF()*.65+bg.blueF()*.35));label->setPalette(palette);
        form->addRow("",label);return label;
    };
    auto addNumber=[&form](const QString &label,int minimum,int maximum,int value,const QString &suffix){
        auto *spin=new QSpinBox;spin->setRange(minimum,maximum);spin->setValue(value);spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);spin->setMinimumWidth(140);spin->setMaximumWidth(180);form->addRow(label,spin);return spin;
    };
    const auto styleNumber=[&](const QString &label,const QString &key,int low,int high,const QString &suffix){
        auto *spin=addNumber(label,low,high,controller.style()[key].toInt(),suffix);
        QObject::connect(spin,&QSpinBox::valueChanged,window,[&controller,key](int v){controller.setStyle(key,v);});return spin;
    };
    const auto styleToggle=[&](const QString &label,const QString &key){
        auto *box=new QCheckBox(label);box->setChecked(controller.style()[key].toBool());form->addRow("",box);
        QObject::connect(box,&QCheckBox::toggled,window,[&controller,key](bool v){controller.setStyle(key,v);});return box;
    };
    const auto styleChoice=[&](const QString &label,const QString &key,const QStringList &labels,const QStringList &values){
        auto *choice=new QComboBox;for(int i=0;i<labels.size();++i)choice->addItem(labels[i],values[i]);
        choice->setCurrentIndex(choice->findData(controller.style()[key]));form->addRow(label,choice);
        QObject::connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),window,[choice,&controller,key](int i){controller.setStyle(key,choice->itemData(i));});return choice;
    };
    const auto styleColor=[&](const QString &label,const QString &key){
        auto *button=new QPushButton;form->addRow(label,button);
        const auto update=[button,&controller,key]{
            const auto text=controller.style()[key].toString();QPixmap swatch(20,20);swatch.fill(QColor(text));
            button->setIcon(QIcon(swatch));button->setText(text.toUpper());
        };update();QObject::connect(&controller,&Controller::settingsChanged,button,update);
        QObject::connect(button,&QPushButton::clicked,window,[window,&controller,key,label]{
            const auto color=QColorDialog::getColor(QColor(controller.style()[key].toString()),window,label);
            if(color.isValid())controller.setStyle(key,color.name());
        });return button;
    };
    auto *effect=new QComboBox;
    effect->addItem("Rainbow","rainbow");effect->addItem("Wallpaper RGB shift","wallpaper-rgb");
    effect->addItem("Solid color","solid");effect->addItem("Custom gradient","gradient");effect->addItem("Intensity colors","intensity");
    effect->addItem("Animated color flow","color-flow");
    effect->setCurrentIndex(effect->findData(controller.effect()));form->addRow("Effect",effect);
    auto *effectHint=hint("");
    const auto updateEffect=[&controller,&wallpaper,effectHint]{
        effectHint->setVisible(controller.effect()=="wallpaper-rgb");
        effectHint->setText(wallpaper.status().isEmpty()?"Reading this monitor’s wallpaper…":wallpaper.status());
    };
    QObject::connect(effect,qOverload<int>(&QComboBox::currentIndexChanged),window,[effect,&controller](int i){controller.setEffect(effect->itemData(i).toString());});
    QObject::connect(&controller,&Controller::settingsChanged,effectHint,updateEffect);
    QObject::connect(&wallpaper,&Wallpaper::changed,effectHint,updateEffect);updateEffect();
    auto *primary=styleColor("Primary / low color","colorA");
    auto *secondary=styleColor("Secondary / high color","colorB");
    auto *gradientDirection=styleChoice("Gradient direction","direction",{"Across spectrum","Bottom to top"},{"horizontal","vertical"});
    const auto updateColors=[&controller,primary,secondary,gradientDirection]{
        const auto effect=controller.effect();
        primary->setEnabled(effect=="solid"||effect=="gradient"||effect=="intensity");
        secondary->setEnabled(effect=="gradient"||effect=="intensity");
        gradientDirection->setEnabled(effect=="gradient");
    };QObject::connect(&controller,&Controller::settingsChanged,window,updateColors);updateColors();
    auto *flowSpeed=styleNumber("Flow speed","flowSpeed",-100,100," %");
    auto *flowDirection=styleChoice("Flow direction","flowDirection",{"Across spectrum","Up the bars"},{"horizontal","vertical"});
    auto *flowHint=hint("Negative speed reverses the flow; zero freezes it.");
    const auto updateFlow=[&controller,form,flowSpeed,flowDirection,flowHint]{
        const bool on=controller.effect()=="color-flow";
        form->setRowVisible(flowSpeed,on);form->setRowVisible(flowDirection,on);form->setRowVisible(flowHint,on);
    };
    QObject::connect(&controller,&Controller::settingsChanged,window,updateFlow);updateFlow();
    auto *opacityRow=new QWidget;auto *opacityLayout=new QHBoxLayout(opacityRow);opacityLayout->setContentsMargins(0,0,0,0);
    auto *transparency=new QSlider(Qt::Horizontal);transparency->setRange(0,100);transparency->setValue(controller.transparency());transparency->setAccessibleName("Transparency");
    auto *transparencyValue=new QSpinBox;transparencyValue->setRange(0,100);transparencyValue->setSuffix(" %");transparencyValue->setValue(controller.transparency());
    opacityLayout->addWidget(transparency,1);opacityLayout->addWidget(transparencyValue);form->addRow("Transparency",opacityRow);
    QObject::connect(transparency,&QSlider::valueChanged,transparencyValue,&QSpinBox::setValue);
    QObject::connect(transparencyValue,&QSpinBox::valueChanged,transparency,&QSlider::setValue);
    QObject::connect(transparencyValue,&QSpinBox::valueChanged,&controller,&Controller::setTransparency);
    form=addPage("Bars");auto *barsPage=tabs->widget(tabs->count()-1);
    auto *width=addNumber("Bar thickness",2,160,controller.barWidth()," px");QObject::connect(width,&QSpinBox::valueChanged,&controller,&Controller::setBarWidth);
    int tallest=150;for(auto *screen:QGuiApplication::screens())tallest=std::max(tallest,screen->size().height());
    auto *height=addNumber("Maximum height",150,std::max(tallest,controller.maxHeight()),controller.maxHeight()==0?400:controller.maxHeight()," px");
    QObject::connect(height,&QSpinBox::valueChanged,&controller,&Controller::setMaxHeight);
    auto *full=new QCheckBox("Full available height");full->setChecked(controller.maxHeight()==0);height->setEnabled(!full->isChecked());form->addRow("",full);
    QObject::connect(full,&QCheckBox::toggled,window,[&controller,height](bool value){height->setEnabled(!value);controller.setMaxHeight(value?0:height->value());});
    auto *segments=new QCheckBox("Segmented bars");segments->setChecked(controller.segmented());form->addRow("",segments);
    QObject::connect(segments,&QCheckBox::toggled,&controller,&Controller::setSegmented);
    auto *segmentHeight=addNumber("Segment height",1,100,controller.segmentHeight()," px");
    auto *segmentGap=addNumber("Segment space",0,50,controller.segmentGap()," px");
    segmentHeight->setEnabled(controller.segmented());segmentGap->setEnabled(controller.segmented());
    QObject::connect(segments,&QCheckBox::toggled,segmentHeight,&QWidget::setEnabled);
    QObject::connect(segments,&QCheckBox::toggled,segmentGap,&QWidget::setEnabled);
    QObject::connect(segmentHeight,&QSpinBox::valueChanged,&controller,&Controller::setSegmentHeight);
    QObject::connect(segmentGap,&QSpinBox::valueChanged,&controller,&Controller::setSegmentGap);
    auto *release=addNumber("Fall time",0,5000,controller.visualizerType()=="bars"?controller.releaseMs():300," ms");
    // Read the original bars setting independently of the selected type.
    release->setValue(controller.barReleaseMs());
    release->setSingleStep(25);release->setSpecialValueText("Immediate (0 ms)");
    QObject::connect(release,&QSpinBox::valueChanged,&controller,&Controller::setReleaseMs);
    const auto modeNumber=[&](const QString &label,const QString &key,int low,int high,const QString &suffix){
        auto *spin=addNumber(label,low,high,controller.visualizerOptions()[key].toInt(),suffix);
        QObject::connect(spin,&QSpinBox::valueChanged,window,[&controller,key](int v){controller.setVisualizerOption(key,v);});return spin;
    };
    const auto modeToggle=[&](const QString &label,const QString &key){
        auto *box=new QCheckBox(label);box->setChecked(controller.visualizerOptions()[key].toBool());form->addRow("",box);
        QObject::connect(box,&QCheckBox::toggled,window,[&controller,key](bool v){controller.setVisualizerOption(key,v);});return box;
    };
    form=addPage("Curve");auto *curvePage=tabs->widget(tabs->count()-1);
    modeNumber("Maximum height","curveHeight",150,16384," px");
    modeNumber("Line thickness","curveLine",1,20," px");
    modeNumber("Smoothing","curveSmooth",0,10,"");
    hint("Blends neighboring frequency bands. Zero preserves sharp peaks.");
    modeToggle("Filled silhouette","curveFill");
    modeNumber("Fall time","curveFall",0,5000," ms");
    form=addPage("Waveform");auto *wavePage=tabs->widget(tabs->count()-1);
    modeNumber("Trace height","waveHeight",150,16384," px");
    modeNumber("Amplitude","waveAmplitude",10,400," %");
    auto *response=new QComboBox;response->addItem("Soft boost","boosted");response->addItem("Linear","linear");
    response->setCurrentIndex(response->findData(controller.visualizerOptions()["waveResponse"]));form->addRow("Response",response);
    QObject::connect(response,qOverload<int>(&QComboBox::currentIndexChanged),window,[&controller,response](int i){controller.setVisualizerOption("waveResponse",response->itemData(i));});
    hint("Soft boost makes quiet waves more visible without changing bass motion.");
    modeNumber("Line thickness","waveLine",1,20," px");
    modeNumber("Audio history","waveHistory",5,200," ms");
    hint("Shorter history spreads out the waveform. Uses the louder stereo channel.");
    modeNumber("Center from top","waveCenter",10,90," %");
    form=addPage("Mirrored");auto *mirrorPage=tabs->widget(tabs->count()-1);auto *mirrorForm=form;
    auto *mirrorShape=new QComboBox;mirrorShape->addItem("Bars","bars");mirrorShape->addItem("Curve","curve");
    mirrorShape->setCurrentIndex(mirrorShape->findData(controller.visualizerOptions()["mirrorStyle"]));form->addRow("Shape",mirrorShape);
    QObject::connect(mirrorShape,qOverload<int>(&QComboBox::currentIndexChanged),window,[&controller,mirrorShape](int i){controller.setVisualizerOption("mirrorStyle",mirrorShape->itemData(i));});
    modeNumber("Total height","mirrorHeight",150,16384," px");
    modeNumber("Center from top","mirrorCenter",10,90," %");
    modeNumber("Lower half scale","mirrorScale",10,200," %");
    hint("100% is symmetrical. Height is limited to the available space.");
    auto *mirrorWidth=modeNumber("Bar thickness","mirrorWidth",2,160," px");
    auto *mirrorGap=modeNumber("Bar spacing","mirrorGap",0,50," px");
    auto *mirrorSegments=modeToggle("Segmented bars","mirrorSegments");
    auto *mirrorSegmentHeight=modeNumber("Segment height","mirrorSegmentHeight",1,100," px");
    auto *mirrorSegmentGap=modeNumber("Segment space","mirrorSegmentGap",0,50," px");
    auto *mirrorLine=modeNumber("Line thickness","mirrorLine",1,20," px");
    auto *mirrorSmooth=modeNumber("Smoothing","mirrorSmooth",0,10,"");
    auto *mirrorFill=modeToggle("Filled silhouette","mirrorFill");
    modeNumber("Fall time","mirrorFall",0,5000," ms");
    const auto updateMirror=[=,&controller]{
        const bool bars=controller.visualizerOptions()["mirrorStyle"]=="bars";
        for(auto *widget:QList<QWidget*>{mirrorWidth,mirrorGap,mirrorSegments,mirrorSegmentHeight,mirrorSegmentGap})mirrorForm->setRowVisible(widget,bars);
        for(auto *widget:QList<QWidget*>{mirrorLine,mirrorSmooth,mirrorFill})mirrorForm->setRowVisible(widget,!bars);
        mirrorSegmentHeight->setEnabled(controller.visualizerOptions()["mirrorSegments"].toBool());
        mirrorSegmentGap->setEnabled(controller.visualizerOptions()["mirrorSegments"].toBool());
    };QObject::connect(&controller,&Controller::settingsChanged,window,updateMirror);updateMirror();
    const auto updateType=[=,&controller]{
        const auto mode=controller.visualizerType();
        tabs->setTabVisible(tabs->indexOf(barsPage),mode=="bars");
        tabs->setTabVisible(tabs->indexOf(curvePage),mode=="curve");
        tabs->setTabVisible(tabs->indexOf(wavePage),mode=="waveform");
        tabs->setTabVisible(tabs->indexOf(mirrorPage),mode=="mirrored");
    };QObject::connect(&controller,&Controller::settingsChanged,window,updateType);updateType();
    form=addPage("Finish");auto *finishForm=form;

    auto *outline=styleToggle("Outline only","outline");
    auto *outlineWidth=styleNumber("Outline width","outlineWidth",1,8," px");
    auto *glow=styleToggle("Neon glow","glow");
    auto *glowRadius=styleNumber("Glow radius","glowRadius",1,30," px");
    auto *glowStrength=styleNumber("Glow strength","glowStrength",0,100," %");
    styleNumber("Vertical fade","fade",0,100," %");
    auto *shading=styleChoice("Surface shading","shading",{"Flat","Metallic","Glass"},{"flat","metal","glass"});
    auto *shadeStrength=styleNumber("Shading strength","shadeStrength",0,100," %");
    auto *peaks=styleToggle("Peak caps","peaks");
    auto *peakColor=styleColor("Peak color","peakColor");
    auto *peakHeight=styleNumber("Peak thickness","peakHeight",1,8," px");
    auto *peakHold=styleNumber("Peak hold","peakHold",0,2000," ms");
    auto *peakFall=styleNumber("Peak fall","peakFall",100,4000," ms");
    const auto updateFinish=[&controller,outlineWidth,glowRadius,glowStrength,shadeStrength,peakColor,peakHeight,peakHold,peakFall]{
        const auto style=controller.style();outlineWidth->setEnabled(style["outline"].toBool());
        glowRadius->setEnabled(style["glow"].toBool());glowStrength->setEnabled(style["glow"].toBool());
        shadeStrength->setEnabled(style["shading"]!="flat");
        const bool p=style["peaks"].toBool();peakColor->setEnabled(p);peakHeight->setEnabled(p);peakHold->setEnabled(p);peakFall->setEnabled(p);
    };QObject::connect(&controller,&Controller::settingsChanged,window,updateFinish);updateFinish();
    const auto updateFinishType=[=,&controller]{
        const bool bars=controller.visualizerType()=="bars";
        for(auto *widget:QList<QWidget*>{outline,outlineWidth,shading,shadeStrength,peaks,peakColor,peakHeight,peakHold,peakFall})
            finishForm->setRowVisible(widget,bars);
    };QObject::connect(&controller,&Controller::settingsChanged,window,updateFinishType);updateFinishType();
    form=addPage("Motion");auto *motionForm=form;
    auto *motion=new QCheckBox("Effect motion");motion->setChecked(controller.motionEnabled());form->addRow("",motion);
    QObject::connect(motion,&QCheckBox::toggled,&controller,&Controller::setMotionEnabled);
    auto *cutoff=addNumber("Bass cutoff",40,500,controller.bassCutoff()," Hz");cutoff->setSingleStep(10);
    cutoff->setEnabled(controller.motionEnabled());
    QObject::connect(motion,&QCheckBox::toggled,cutoff,&QWidget::setEnabled);
    QObject::connect(cutoff,&QSpinBox::valueChanged,&controller,&Controller::setBassCutoff);
    hint("Frequencies below this drive motion and are removed from spectrum modes. The waveform keeps the full signal.");
    auto *motionType=new QComboBox;motionType->addItem("Zoom","zoom");motionType->addItem("Shake","shake");
    motionType->setCurrentIndex(motionType->findData(controller.motionType()));form->addRow("Motion type",motionType);
    QObject::connect(motionType,qOverload<int>(&QComboBox::currentIndexChanged),window,[motionType,&controller](int index){controller.setMotionType(motionType->itemData(index).toString());});
    auto *depth=addNumber("Zoom depth",0,40,controller.bassDepth()," %");
    QObject::connect(depth,&QSpinBox::valueChanged,&controller,&Controller::setBassDepth);
    auto *depthHint=hint("Maximum enlargement on a bass hit.");
    auto *shake=addNumber("Shake strength",0,40,controller.shakeStrength()," px");
    QObject::connect(shake,&QSpinBox::valueChanged,&controller,&Controller::setShakeStrength);
    auto *shakeHint=hint("Maximum sideways travel; vertical movement is smaller.");
    const auto updateMotion=[&controller,motionForm,motionType,depth,depthHint,shake,shakeHint]{
        const bool zoom=controller.motionType()=="zoom";
        motionType->setEnabled(controller.motionEnabled());
        depth->setEnabled(controller.motionEnabled());shake->setEnabled(controller.motionEnabled());
        motionForm->setRowVisible(depth,zoom);motionForm->setRowVisible(depthHint,zoom);
        motionForm->setRowVisible(shake,!zoom);motionForm->setRowVisible(shakeHint,!zoom);
    };
    QObject::connect(&controller,&Controller::settingsChanged,window,updateMotion);updateMotion();
    hint("Turn motion off to restore bass in the spectrum. Zero strength only stops movement.");
    form=addPage("Audio");
    auto *gain=new QDoubleSpinBox;gain->setRange(.1,20);gain->setDecimals(1);gain->setSingleStep(.1);gain->setSuffix(" ×");
    gain->setValue(controller.audioGain());gain->setKeyboardTracking(false);form->addRow("Audio gain",gain);
    QObject::connect(gain,&QDoubleSpinBox::valueChanged,&controller,&Controller::setAudioGain);
    hint("1.0× is unchanged. Boosts visualization sensitivity, not audible volume.");
    auto *hz=addNumber("Analysis rate",10,120,controller.analysisHz()," Hz");QObject::connect(hz,&QSpinBox::valueChanged,&controller,&Controller::setAnalysisHz);
    hint("Spectrum updates per second.");

    form=addPage("Behavior");
    auto *automatic=new QCheckBox("Follow application playback");automatic->setChecked(controller.automatic());form->addRow("Automatic",automatic);
    QObject::connect(automatic,&QCheckBox::toggled,&controller,&Controller::setAutomatic);
    auto *sourceRow=new QWidget;auto *sourceLayout=new QHBoxLayout(sourceRow);sourceLayout->setContentsMargins(0,0,0,0);
    auto *source=new QComboBox;source->setEditable(true);source->setInsertPolicy(QComboBox::NoInsert);source->setMinimumContentsLength(20);source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    source->completer()->setCaseSensitivity(Qt::CaseInsensitive);source->completer()->setFilterMode(Qt::MatchContains);
    auto *refresh=new QPushButton("Refresh");sourceLayout->addWidget(source,1);sourceLayout->addWidget(refresh);auto *clearSource=new QPushButton("Clear");sourceLayout->addWidget(clearSource);form->addRow("Application",sourceRow);
    const auto refill=[source,&controller]{
        QSignalBlocker blocker(source);source->clear();source->addItem("Desktop audio (all applications)",QString());
        const auto selected=controller.selectedExecutable();auto paths=ActivityMonitor::runningExecutables();
        if(!selected.isEmpty()&&!paths.contains(selected))paths.prepend(selected);
        for(const auto &path:paths){source->addItem(QFileInfo(path).fileName()+" — "+QFileInfo(path).absolutePath(),path);source->setItemData(source->count()-1,path,Qt::ToolTipRole);}
        source->setCurrentIndex(std::max(0,source->findData(selected)));
    };refill();
    QObject::connect(refresh,&QPushButton::clicked,window,refill);
    QObject::connect(clearSource,&QPushButton::clicked,window,[&controller,refill]{controller.setSelectedExecutable("");refill();});
    QObject::connect(source,qOverload<int>(&QComboBox::activated),window,[source,&controller](int index){controller.setSelectedExecutable(source->itemData(index).toString());});
    auto *activity=hint("");
    const auto refreshActivity=[activity,&controller]{activity->setText(!controller.enabled()?"Disabled manually":controller.selectedExecutable().isEmpty()?"Captures desktop audio. Select an app to isolate its audio.":controller.automatic()?controller.activityStatus():"Captures only the selected app. Remembers it across restarts.");};
    refreshActivity();
    automatic->setToolTip("Manual off always overrides automatic playback detection.");
    QObject::connect(&controller,&Controller::activityStatusChanged,activity,refreshActivity);
    QObject::connect(&controller,&Controller::enabledChanged,activity,refreshActivity);
    QObject::connect(&controller,&Controller::settingsChanged,activity,refreshActivity);
    auto *fit=new QCheckBox("Fit to visible desktop");fit->setChecked(controller.fitDesktop());form->addRow("Placement",fit);
    QObject::connect(fit,&QCheckBox::toggled,&controller,&Controller::setFitDesktop);
    auto *fitHint=hint("Shares one spectrum across open areas, from left to right.");
    const auto updateFit=[fitHint,&controller]{
        const auto text=controller.desktopLayout()->status();
        fitHint->setText(!controller.fitDesktop()||text.isEmpty()?"Shares one spectrum across open areas, from left to right.":text);
    };
    QObject::connect(controller.desktopLayout(),&DesktopLayout::statusChanged,fitHint,updateFit);
    QObject::connect(&controller,&Controller::settingsChanged,fitHint,updateFit);
    // Runtime errors belong beside their relevant control, not in a prose footer.
    QObject::connect(&controller,&Controller::statusChanged,activity,[activity](const QString &text){if(text.contains("failed",Qt::CaseInsensitive)||text.contains("stopped (")||text.startsWith("Audio:")||text.startsWith("Audio helper:")||text.startsWith("Could not"))activity->setText(text);});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);auto *quit=buttons->addButton("Quit visualizer",QDialogButtonBox::DestructiveRole);
    QObject::connect(quit,&QPushButton::clicked,qApp,&QCoreApplication::quit);QObject::connect(buttons,&QDialogButtonBox::rejected,window,&QWidget::close);layout->addWidget(buttons);
    return window;
}
