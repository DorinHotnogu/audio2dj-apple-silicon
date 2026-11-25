#pragma once
// 5 ms linear ramp at the fixed 48 kHz USB sample rate.
struct GainSmoother {
 double current=0.1,target=0.1,step=0;unsigned remaining=0;
 double next(double requested){
  if(requested!=target){target=requested;remaining=240;step=(target-current)/240.0;}
  if(remaining){current+=step;if(!--remaining)current=target;}
  return current;
 }
};
