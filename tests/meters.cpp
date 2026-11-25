#include "../src/PeakMeter.h"
#include <cassert>
#include <cstdio>
int main(){PeakMeter channels[4];channels[0].observe(-.5f);channels[0].observe(.2f);channels[1].observe(.25f);assert(channels[0].take()==.5f);assert(channels[1].take()==.25f);assert(channels[2].take()==0&&channels[3].take()==0);assert(channels[0].take()==0);puts("PASS: independent stereo-channel peaks, signed samples, interval reset");}
