#pragma once
#include <algorithm>

// Linear release reaches the new target within the selected duration after
// a drop. Its timing is independent of the analysis rate and rendering cadence.
class Envelope {
public:
    double value() const { return current; }
    void setTarget(double next, int releaseMs) {
        next = std::clamp(next,0.0,1.0);
        if(next < target) speed = releaseMs ? std::max(0.0,current-next)*1000/releaseMs : 0;
        target = next;
    }
    void setRelease(int releaseMs) {
        speed = releaseMs ? std::max(0.0,current-target)*1000/releaseMs : 0;
    }
    double advance(double seconds, int releaseMs) {
        current = releaseMs > 0 && target < current
            ? std::max(target,current-speed*seconds) : target;
        return current;
    }
private:
    double target = 0, current = 0, speed = 0;
};
