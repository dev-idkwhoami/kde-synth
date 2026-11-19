#pragma once
#include <QVariantMap>
#include <QStringList>
#include <algorithm>
#include <cmath>

inline QVariantMap defaultVisualizers(){
    return {{"curveHeight",400},{"curveLine",3},{"curveSmooth",3},{"curveFill",true},{"curveFall",300},
        {"waveResponse","boosted"},{"waveHeight",300},{"waveLine",3},{"waveAmplitude",100},{"waveHistory",20},{"waveCenter",50},
        {"mirrorHeight",500},{"mirrorLine",3},{"mirrorSmooth",3},{"mirrorFill",true},{"mirrorFall",300},
        {"mirrorStyle","bars"},{"mirrorWidth",24},{"mirrorGap",4},{"mirrorCenter",50},{"mirrorScale",100},
        {"mirrorSegments",false},{"mirrorSegmentHeight",6},{"mirrorSegmentGap",3}};
}
inline QVariant validateVisualizer(const QString &key,const QVariant &value){
    if(key=="waveResponse")return QStringList{"linear","boosted"}.contains(value.toString())?value:QVariant();
    if(key=="mirrorStyle")return QStringList{"bars","curve"}.contains(value.toString())?value:QVariant();
    if(QStringList{"curveFill","mirrorFill","mirrorSegments"}.contains(key))return value.toBool();
    const QMap<QString,QPair<int,int>> ranges={
        {"curveHeight",{150,16384}},{"curveLine",{1,20}},{"curveSmooth",{0,10}},{"curveFall",{0,5000}},
        {"waveHeight",{150,16384}},{"waveLine",{1,20}},{"waveAmplitude",{10,400}},{"waveHistory",{5,200}},{"waveCenter",{10,90}},
        {"mirrorHeight",{150,16384}},{"mirrorLine",{1,20}},{"mirrorSmooth",{0,10}},{"mirrorFall",{0,5000}},
        {"mirrorWidth",{2,160}},{"mirrorGap",{0,50}},{"mirrorCenter",{10,90}},{"mirrorScale",{10,200}},
        {"mirrorSegmentHeight",{1,100}},{"mirrorSegmentGap",{0,50}}};
    if(!ranges.contains(key))return {};
    bool ok=false;const int n=value.toInt(&ok);if(!ok)return {};
    return std::clamp(n,ranges[key].first,ranges[key].second);
}


// Fixed display compression only. Never feeds spectrum analysis or bass motion.
inline double waveformDisplaySample(double value,bool boosted){
    return boosted?std::copysign(std::sqrt(std::abs(value)),value):value;
}
