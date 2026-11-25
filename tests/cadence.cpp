#include "../src/PCMBuffer.h"
#include <cstdio>
#include <cassert>
static StereoPCMBuffer q;
int main(){static float block[2800];for(auto& x:block)x=.2;double next=0;unsigned batch=0;float out[2];unsigned audible=0;for(unsigned usb=0;usb<48000*20;usb++){double ms=usb/48.0;if(ms>=next){q.push(block,1024,44100);q.push(block+2048,376,44100);batch++;double jitter=(batch%4==0?4:batch%4==1?-4:0);next=batch*1400.0/44.1+jitter;}q.next(out,1,1536);if(out[0]!=0)audible++;}printf("32 ms reserve, 1400-frame FL blocks at 44.1 kHz, +/-4 ms jitter: underruns=%llu audible=%u\n",q.underruns.load(),audible);assert(q.underruns.load()==0&&audible>900000);}
