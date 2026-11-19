#pragma once
#include <algorithm>
class PeakHold {
public:
    double update(double level,double seconds,int holdMs,int fallMs){
        if(level>=m_value){m_value=level;m_hold=holdMs/1000.0;return m_value;}
        const double falling=std::max(0.0,seconds-m_hold);
        m_hold=std::max(0.0,m_hold-seconds);
        m_value=std::max(level,m_value-falling*1000/fallMs);
        return m_value;
    }
private:
    double m_value=0,m_hold=0;
};
