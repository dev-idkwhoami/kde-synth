#pragma once
#include <QVariantMap>
#include <QRegularExpression>
#include <algorithm>

inline QVariantMap defaultStyle(){
    return {{"colorA","#36d5ff"},{"colorB","#ff388b"},{"direction","vertical"},
        {"outline",false},{"outlineWidth",2},{"glow",false},{"glowRadius",10},{"glowStrength",50},
        {"fade",0},{"shading","flat"},{"shadeStrength",60},
        {"flowSpeed",20},{"flowDirection","horizontal"},
        {"peaks",false},{"peakHold",300},{"peakFall",900},{"peakHeight",3},{"peakColor","#ffffff"}};
}
inline QVariant validatedStyle(const QString &key,const QVariant &value){
    if(key=="colorA"||key=="colorB"||key=="peakColor"){
        return QRegularExpression("^#[0-9a-fA-F]{6}$").match(value.toString()).hasMatch()?value:QVariant();
    }
    if(key=="direction"||key=="flowDirection")return value.toString()=="horizontal"||value.toString()=="vertical"?value:QVariant();
    if(key=="shading")return QStringList{"flat","metal","glass"}.contains(value.toString())?value:QVariant();
    if(QStringList{"outline","glow","peaks"}.contains(key))return value.toBool();
    const QMap<QString,QPair<int,int>> limits={
        {"outlineWidth",{1,8}},{"glowRadius",{1,30}},{"glowStrength",{0,100}},{"fade",{0,100}},
        {"shadeStrength",{0,100}},{"flowSpeed",{-100,100}},{"peakHold",{0,2000}},
        {"peakFall",{100,4000}},{"peakHeight",{1,8}}};
    if(!limits.contains(key))return {};
    bool ok=false;int n=value.toInt(&ok);if(!ok)return {};
    const auto range=limits[key];return std::clamp(n,range.first,range.second);
}
