#pragma once
#include <atomic>
#include <cmath>
struct PeakMeter {
 std::atomic<float> peak{0};
 void observe(float sample){float value=std::fabs(sample),old=peak.load(std::memory_order_relaxed);while(value>old&&!peak.compare_exchange_weak(old,value,std::memory_order_relaxed)){} }
 float take(){return peak.exchange(0,std::memory_order_relaxed);}
};
