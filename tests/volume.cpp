#include "../src/GainSmoother.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){GainSmoother g;double before=.1;for(int i=0;i<240;i++){double v=g.next(1);assert(v>=before&&v-before<=.9/240+1e-12);before=v;}assert(g.current==1&&g.remaining==0);for(int i=0;i<100;i++)g.next(0);double v=g.current;g.next(.6);assert(fabs(g.current-v)<.005);for(int i=0;i<240;i++)g.next(0);assert(g.current==0);puts("PASS: 5 ms bounded volume ramp, monotonic movement, rapid reversal, exact mute");}
