#pragma once
#include "Envelope.h"
#include <array>
#include <cmath>

class BassMotion {
public:
    // Split at the next logarithmic band edge so motion and bars never
    // intentionally share a band. FFT resolution makes this approximate.
    static int bandCount(int cutoffHz){
        return std::clamp(static_cast<int>(std::ceil(128*std::log(cutoffHz/30.0)/std::log(16000.0/30))),1,127);
    }
    void update(const std::array<double,128> &bands,int cutoffHz=150){
        double peak=0;
        for(int i=0;i<bandCount(cutoffHz);++i)peak=std::max(peak,bands[i]);
        // dB-normalized noise should not leave the scene permanently enlarged.
        const double strength=std::clamp((peak-.35)/.65,0.0,1.0);
        envelope.setTarget(strength*strength,180);
    }
    void silence(){envelope.setTarget(0,180);}
    double advance(double seconds){return envelope.advance(seconds,180);}
private:
    Envelope envelope;
};
