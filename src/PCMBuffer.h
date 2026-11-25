#pragma once
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdint>
struct StereoPCMBuffer {
 static constexpr uint64_t capacity=65536;
 float samples[capacity][2]{};
 std::atomic<uint64_t> written{0},read{0},dropped{0},underruns{0};
 std::atomic<unsigned> rate{48000},largestBlock{0};
 double fraction=0;bool primed=false;
 void push(const float* data,unsigned frames,unsigned hz){
  auto w=written.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
  if(w-r+frames>capacity){dropped+=frames;return;}
  for(unsigned f=0;f<frames;f++)for(int c=0;c<2;c++){float v=data[f*2+c];samples[(w+f)%capacity][c]=std::isfinite(v)?v:0;}
  auto largest=largestBlock.load();if(frames>largest)largestBlock.store(frames);
  rate.store(hz);written.store(w+frames,std::memory_order_release);
 }
 void next(float* out,double gain,unsigned target){
  auto r=read.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire),fill=w-r;
  const double reserve=target*(rate.load()/48000.0);const double block=largestBlock.load();
  if(!primed){if(fill<reserve+block){out[0]=out[1]=0;return;}primed=true;}
  if(fill<2){out[0]=out[1]=0;primed=false;fraction=0;underruns++;return;}
  double ratio=(rate.load()/48000.0)*std::clamp(1.0+((double)fill-(reserve+block*0.5))*0.0000002,0.998,1.002);
  for(int c=0;c<2;c++){double a=samples[r%capacity][c],b=samples[(r+1)%capacity][c];out[c]=std::clamp((a+(b-a)*fraction)*gain,-1.0,1.0);}
  fraction+=ratio;auto advance=(uint64_t)fraction;fraction-=advance;read.store(r+advance,std::memory_order_release);
 }
};
