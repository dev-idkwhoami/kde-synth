#pragma once
#include <QRectF>
#include <QVector>
#include <algorithm>

// Largest axis-aligned rectangle in bounds not covered by any obstacle.
// Candidate vertical sides lie on bounds or obstacle edges.
inline QRectF largestFreeArea(const QRectF &bounds,const QVector<QRectF> &obstacles){
    QVector<QRectF> clipped;QVector<double> xs{bounds.left(),bounds.right()};
    for(const auto &obstacle:obstacles){auto r=obstacle.intersected(bounds);if(r.isEmpty())continue;clipped.append(r);xs<<r.left()<<r.right();}
    std::sort(xs.begin(),xs.end());xs.erase(std::unique(xs.begin(),xs.end()),xs.end());
    QRectF best;double bestArea=0;
    for(int i=0;i<xs.size();++i)for(int j=i+1;j<xs.size();++j){
        const double width=xs[j]-xs[i];if(width<80)continue;
        QVector<QPair<double,double>> blocked;
        for(const auto &r:clipped)if(r.left()<xs[j]&&r.right()>xs[i])blocked.append({r.top(),r.bottom()});
        std::sort(blocked.begin(),blocked.end());
        double y=bounds.top();
        auto consider=[&](double bottom){const double height=bottom-y,area=width*height;if(height>=48&&area>bestArea){bestArea=area;best=QRectF(xs[i],y,width,height);}};
        for(const auto &span:blocked){consider(span.first);y=std::max(y,span.second);}
        consider(bounds.bottom());
    }
    return best;
}
