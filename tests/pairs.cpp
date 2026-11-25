#include "../src/PCMBuffer.h"
#include <cassert>
#include <cstdio>
static StereoPCMBuffer pair[2];
int main(){float a[8192],b[8192];for(int i=0;i<4096;i++){a[i*2]=.2;a[i*2+1]=.3;b[i*2]=-.4;b[i*2+1]=-.5;}for(int j=0;j<4;j++){pair[0].push(a+j*2048,1024,44100);pair[1].push(b+j*2048,1024,48000);}float x[2],y[2];for(int i=0;i<2000;i++){pair[0].next(x,.5,2048);pair[1].next(y,.25,2048);assert(fabs(x[0]-.1)<1e-6&&fabs(x[1]-.15)<1e-6);assert(fabs(y[0]+.1)<1e-6&&fabs(y[1]+.125)<1e-6);}assert(pair[0].read.load()<pair[1].read.load());for(int i=0;i<8000;i++){if(i%1000==0)pair[1].push(b,1000,48000);pair[0].next(x,1,2048);pair[1].next(y,.25,2048);}assert(x[0]==0&&x[1]==0);assert(fabs(y[0]+.1)<1e-6);assert(pair[0].underruns.load()>0&&pair[1].underruns.load()==0);puts("PASS: independent A/B PCM, distinct rates and gains; A starvation does not interrupt B");}
