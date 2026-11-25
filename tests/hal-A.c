#include "../src/DriverA.c"
#include <assert.h>
#include <math.h>
static OSStatus storage(AudioServerPlugInHostRef h,CFStringRef k,CFPropertyListRef* out){*out=NULL;return 0;}
int main(void){
 AudioServerPlugInHostInterface host={0};host.CopyFromStorage=storage;
 assert(Audio2DJ_Initialize(gAudioServerPlugInDriverRef,&host)==0);
 assert(gDevice_SampleRate==48000.0);
 mach_port_t receive;assert(mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE,&receive)==KERN_SUCCESS);
 assert(mach_port_insert_right(mach_task_self(),receive,receive,MACH_MSG_TYPE_MAKE_SEND)==KERN_SUCCESS);a2port=receive;
 float pcm[512];for(int i=0;i<512;i++)pcm[i]=sinf(i*0.01f)*0.01f;
 AudioServerPlugInIOCycleInfo cycle={0};
 assert(Audio2DJ_DoIOOperation(gAudioServerPlugInDriverRef,kObjectID_Device,kObjectID_Stream_Output,1,kAudioServerPlugInIOOperationWriteMix,256,&cycle,pcm,NULL)==0);
 struct{A2Message msg;mach_msg_max_trailer_t trailer;} result={0};
 assert(mach_msg(&result.msg.header,MACH_RCV_MSG|MACH_RCV_TIMEOUT,0,sizeof(result),receive,100,MACH_PORT_NULL)==KERN_SUCCESS);
 assert(result.msg.version==3&&result.msg.pair==A2_PAIR&&result.msg.frames==256);for(int i=0;i<512;i++)assert(result.msg.samples[i]==pcm[i]);
 atomic_store(&a2muted,true);a2send(pcm,256);
 assert(mach_msg(&result.msg.header,MACH_RCV_MSG|MACH_RCV_TIMEOUT,0,sizeof(result),receive,100,MACH_PORT_NULL)==KERN_SUCCESS);
 for(int i=0;i<512;i++)assert(result.msg.samples[i]==0);
 AudioObjectPropertyAddress address={kAudioObjectPropertyControlList,kAudioObjectPropertyScopeGlobal,0};UInt32 size=0;assert(Audio2DJ_GetDevicePropertyDataSize(gAudioServerPlugInDriverRef,kObjectID_Device,0,&address,0,NULL,&size)==0&&size==8);
 AudioObjectID controls[2];assert(Audio2DJ_GetDevicePropertyData(gAudioServerPlugInDriverRef,kObjectID_Device,0,&address,0,NULL,sizeof(controls),&size,controls)==0);assert(controls[0]==kObjectID_Volume_Output_Master&&controls[1]==kObjectID_Mute_Output_Master);
 address.mSelector=kAudioStreamPropertyPhysicalFormat;AudioStreamBasicDescription format;assert(Audio2DJ_GetStreamPropertyData(gAudioServerPlugInDriverRef,kObjectID_Stream_Output,0,&address,0,NULL,sizeof(format),&size,&format)==0);assert(format.mChannelsPerFrame==2&&format.mBytesPerFrame==8);
 address.mSelector=kAudioStreamPropertyAvailablePhysicalFormats;AudioStreamRangedDescription formats[2];assert(Audio2DJ_GetStreamPropertyData(gAudioServerPlugInDriverRef,kObjectID_Stream_Output,0,&address,0,NULL,sizeof(formats),&size,formats)==0);assert(formats[0].mFormat.mSampleRate==44100&&formats[1].mFormat.mSampleRate==48000);
 puts("PASS: stereo device, 44.1/48 kHz, no dummy selectors, isolated Mach PCM and mute");return 0;
}
