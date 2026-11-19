#pragma once
#include <QMap>
#include <QRectF>
#include <QVariantMap>
#include <QVector>
#include <algorithm>

// Concatenate visible widths, ignoring covered areas and physical monitor gaps.
// Coordinates are global logical desktop coordinates, never connector-name order.
inline QVariantMap spectrumSlices(const QMap<QString,QRectF> &regions){
    struct Region { QString name; QRectF rect; };
    QVector<Region> ordered;
    double total=0;
    for(auto it=regions.cbegin();it!=regions.cend();++it){
        if(it.value().isEmpty())continue;
        ordered.append({it.key(),it.value()});total+=it.value().width();
    }
    std::sort(ordered.begin(),ordered.end(),[](const Region &a,const Region &b){
        if(a.rect.x()!=b.rect.x())return a.rect.x()<b.rect.x();
        if(a.rect.y()!=b.rect.y())return a.rect.y()<b.rect.y();
        return a.name<b.name;
    });
    QVariantMap result;
    double offset=0;
    for(const auto &region:ordered){
        const double start=offset/total;
        offset+=region.rect.width();
        result.insert(region.name,QVariantMap{{"start",start},{"end",offset/total}});
    }
    return result;
}
