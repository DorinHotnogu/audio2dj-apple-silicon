#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "Bridge.h"
#include <atomic>
#include <thread>
#include <algorithm>
#include <signal.h>
#include <bsm/libbsm.h>
#include <unistd.h>
#include "PCMBuffer.h"
#include "GainSmoother.h"
#include "PeakMeter.h"
static PeakMeter levels[4];
#include <chrono>
#include <sys/stat.h>
#include <pwd.h>
#include <string>
static std::string settingsPath(const char* name){auto user=getpwuid(geteuid());return std::string(user?user->pw_dir:"/nonexistent")+"/Library/Application Support/Audio2DJ/"+name;}
static StereoPCMBuffer pairs[2];
static volatile sig_atomic_t stopRequested=0;
static void onSignal(int){stopRequested=1;}
static std::atomic<double> gainA{0.1},gainB{0.1};static std::atomic<unsigned> targetFrames{3072},usbAhead{16};
static float heldPCM[4]={};
static GainSmoother volumeA,volumeB;
static void nextPCM(){pairs[0].next(heldPCM,volumeA.next(gainA.load()),targetFrames.load());pairs[1].next(heldPCM+2,volumeB.next(gainB.load()),targetFrames.load());for(int channel=0;channel<4;channel++)levels[channel].observe(heldPCM[channel]);}
static void receivePCM(){
 mach_port_t port=MACH_PORT_NULL;auto kr=bootstrap_check_in(bootstrap_port,A2_SERVICE,&port);
 if(kr){fprintf(stderr,"Mach service check-in failed: %s\n",mach_error_string(kr));stopRequested=1;return;}
 struct {A2Message m;mach_msg_max_trailer_t trailer;} buf;
 while(!stopRequested){memset(&buf,0,sizeof(buf));auto status=mach_msg(&buf.m.header,MACH_RCV_MSG|MACH_RCV_TIMEOUT|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)|MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),0,sizeof(buf),port,500,MACH_PORT_NULL);if(status==MACH_RCV_TIMED_OUT)continue;if(status)continue;
 auto& m=buf.m;if(m.header.msgh_bits&MACH_MSGH_BITS_COMPLEX){mach_msg_destroy(&m.header);continue;}auto trailer=(mach_msg_audit_trailer_t*)((char*)&m+((m.header.msgh_size+3)&~3));if(trailer->msgh_trailer_size<sizeof(mach_msg_audit_trailer_t))continue;auto sender=audit_token_to_euid(trailer->msgh_audit);if(sender!=202&&sender!=geteuid())continue;if(m.header.msgh_id!=A2_MESSAGE_ID||m.version!=3||m.pair>1||m.frames>A2_FRAMES||m.header.msgh_size!=offsetof(A2Message,samples)+m.frames*8)continue;
 if(m.sampleRate!=44100&&m.sampleRate!=48000)continue;
 pairs[m.pair].push(m.samples,m.frames,m.sampleRate);
 }
}

static IOUSBInterfaceInterface245** audioif;
struct Transfer {unsigned char data[4096];IOUSBIsocFrame frames[8];unsigned char* lowData=nullptr;IOUSBLowLatencyIsocFrame* lowFrames=nullptr;bool busy=false;};
static Transfer reads[32],writes[256];static bool running=false;static UInt64 nextRead,nextWrite;static unsigned received=0,sent=0,errors=0,late=0,submitErrors=0,completionErrors=0;static unsigned long sampleByte=4;
static unsigned frameErrors=0,dropped=0;
static void wrote(void* ref,IOReturn status,void*){((Transfer*)ref)->busy=false;if(!running)return;if(status){errors++;completionErrors++;if(completionErrors<4)printf("write completion=%08x\n",status);}else {sent++;for(auto& f:((Transfer*)ref)->frames)if(f.frStatus){frameErrors++;if(frameErrors<5)printf("output frame status=%08x requested=%u actual=%u\n",f.frStatus,f.frReqCount,f.frActCount);}}}
static void submitRead(Transfer*);
static void readDone(void* ref,IOReturn status,void*){
 auto t=(Transfer*)ref;if(!running)return;if(status)errors++;else {
 received++;if(received==500){printf("feedback bytes:");for(int z=0;z<32;z++)printf(" %02x",t->data[z]);puts("");}if(received<4){for(int k=0;k<8;k++)printf("frame %d status=%08x actual=%u\n",k,t->frames[k].frStatus,t->frames[k].frActCount);}
 Transfer* o=nullptr;for(auto& w:writes)if(!w.busy){o=&w;break;}
 if(!o)dropped++;
 if(o){unsigned offset=0;memset(o->data,0,sizeof(o->data));for(int f=0;f<8;f++){
 unsigned len=t->frames[f].frActCount;if(len>512)len=0;
 o->frames[f]={};o->frames[f].frReqCount=len;
 for(unsigned i=0;i<len;){
 for(unsigned stream=0;stream<2&&i<len;stream++,i++){
 int byte=sampleByte%6;if(stream==0 && byte==0)nextPCM();int val=(int)(8388607.0*heldPCM[stream*2+byte/3]);
 o->data[offset+i]=(val >> (8*(2-byte%3)))&255;
 }
 sampleByte++;
 if(i%8==4)for(unsigned stream=0;stream<2&&i<len;stream++,i++)o->data[offset+i]=(stream<<1)|(~(i/8)&1);
 }
 offset+=len;}
 if(offset){UInt64 now;AbsoluteTime at;(*audioif)->GetBusFrameNumber(audioif,&now,&at);if(!nextWrite)nextWrite=now+usbAhead.load();else if(nextWrite<now+4){late++;nextWrite=now+usbAhead.load();}o->busy=true;memcpy(o->lowData,o->data,offset);for(int k=0;k<8;k++){o->lowFrames[k]={};o->lowFrames[k].frReqCount=o->frames[k].frReqCount;}auto r=(*audioif)->LowLatencyWriteIsochPipeAsync(audioif,4,o->lowData,nextWrite++,8,0,o->lowFrames,wrote,o);if(r){o->busy=false;errors++;submitErrors++;if(submitErrors<4)printf("write submit %08x\n",r);}}
 }
 }submitRead(t);
}
static void submitRead(Transfer* t){for(auto& f:t->frames){f={};f.frReqCount=512;}UInt64 now;AbsoluteTime at;(*audioif)->GetBusFrameNumber(audioif,&now,&at);if(nextRead<now+8)nextRead=now+8;auto r=(*audioif)->ReadIsochPipeAsync(audioif,3,t->data,nextRead++,8,t->frames,readDone,t);if(r){errors++;if(errors<4)printf("read submit %08x\n",r);}}

static void loadSettings(){
 static timespec lastModified={};struct stat metadata={};
 if(stat(settingsPath("settings.plist").c_str(),&metadata)==0&&(metadata.st_mtimespec.tv_sec!=lastModified.tv_sec||metadata.st_mtimespec.tv_nsec!=lastModified.tv_nsec)){lastModified=metadata.st_mtimespec;
 auto path=settingsPath("settings.plist");CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)path.c_str(),path.size(),false);
 CFReadStreamRef stream=CFReadStreamCreateWithFile(NULL,url);CFRelease(url);
 if(stream&&CFReadStreamOpen(stream)){auto obj=CFPropertyListCreateWithStream(NULL,stream,0,kCFPropertyListImmutable,NULL,NULL);if(obj&&CFGetTypeID(obj)==CFDictionaryGetTypeID()){
 auto d=(CFDictionaryRef)obj;auto number=[&](CFStringRef key,double fallback){auto v=CFDictionaryGetValue(d,key);double result=fallback;if(v&&CFGetTypeID(v)==CFNumberGetTypeID())CFNumberGetValue((CFNumberRef)v,kCFNumberDoubleType,&result);return std::isfinite(result)?result:fallback;};
 gainA.store(std::clamp(number(CFSTR("gainA"),0.1),0.0,1.0));gainB.store(std::clamp(number(CFSTR("gainB"),0.1),0.0,1.0));double ms=std::clamp(number(CFSTR("bufferMS"),64),32.0,128.0);targetFrames.store((unsigned)(ms*48));usbAhead.store(ms<=32?8:16);
 }if(obj)CFRelease(obj);CFReadStreamClose(stream);}if(stream)CFRelease(stream);
 }
 static auto lastStatus=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(now-lastStatus<std::chrono::milliseconds(50))return;lastStatus=now;
 FILE* f=fopen(settingsPath("status.json.tmp").c_str(),"w");if(f){fprintf(f,"{\"underruns\":%llu,\"droppedFrames\":%llu,\"bufferedFrames\":%llu,\"peakAL\":%.6f,\"peakAR\":%.6f,\"peakBL\":%.6f,\"peakBR\":%.6f}",(unsigned long long)(pairs[0].underruns.load()+pairs[1].underruns.load()),(unsigned long long)(pairs[0].dropped.load()+pairs[1].dropped.load()),(unsigned long long)(pairs[0].written.load()-pairs[0].read.load()+pairs[1].written.load()-pairs[1].read.load()),levels[0].take(),levels[1].take(),levels[2].take(),levels[3].take());fclose(f);rename(settingsPath("status.json.tmp").c_str(),settingsPath("status.json").c_str());}
}

static void play(IOUSBInterfaceInterface245** iface,UInt8 in,UInt8 out){
 audioif=iface;for(auto& w:writes){auto a=(*iface)->LowLatencyCreateBuffer(iface,(void**)&w.lowData,4096,kUSBLowLatencyWriteBuffer);auto b=(*iface)->LowLatencyCreateBuffer(iface,(void**)&w.lowFrames,8*sizeof(IOUSBLowLatencyIsocFrame),kUSBLowLatencyFrameListBuffer);if(a||b){printf("allocate %08x %08x\n",a,b);return;}}unsigned char cmd[]={9,1,2,176,0,1},reply[64];UInt32 n=64;
 auto r=(*iface)->WritePipe(iface,out,cmd,sizeof(cmd));if(!r)r=(*iface)->ReadPipeTO(iface,in,reply,&n,1000,1000);
 printf("audio params status=%08x bytes=%u reply=%02x %02x\n",r,(unsigned)n,reply[0],reply[1]);if(r||n<2||reply[0]!=9||reply[1]!=1)return;
 CFRunLoopSourceRef source=nullptr;r=(*iface)->CreateInterfaceAsyncEventSource(iface,&source);if(r){printf("async source %08x\n",r);return;}
 CFRunLoopAddSource(CFRunLoopGetCurrent(),source,kCFRunLoopDefaultMode);running=true;for(auto& t:reads)submitRead(&t);
 while(!stopRequested)CFRunLoopRunInMode(kCFRunLoopDefaultMode,1,false);running=false;(*iface)->AbortPipe(iface,3);(*iface)->AbortPipe(iface,4);CFRunLoopRunInMode(kCFRunLoopDefaultMode,.1,false);CFRunLoopRemoveSource(CFRunLoopGetCurrent(),source,kCFRunLoopDefaultMode);CFRelease(source);for(auto& w:writes){(*iface)->LowLatencyDestroyBuffer(iface,w.lowData);(*iface)->LowLatencyDestroyBuffer(iface,w.lowFrames);}printf("received=%u sent=%u errors=%u late=%u submit=%u completion=%u frameErrors=%u dropped=%u\n",received,sent,errors,late,submitErrors,completionErrors,frameErrors,dropped);
}

int main(){
 signal(SIGTERM,onSignal);signal(SIGINT,onSignal);loadSettings();std::thread([]{while(!stopRequested){loadSettings();std::this_thread::sleep_for(std::chrono::milliseconds(20));}}).detach();std::thread(receivePCM).detach();
 auto match=IOServiceMatching(kIOUSBDeviceClassName);
 int vendor=0x17cc, product=0x041c;
 auto v=CFNumberCreate(nullptr,kCFNumberIntType,&vendor),p=CFNumberCreate(nullptr,kCFNumberIntType,&product);
 CFDictionarySetValue(match,CFSTR(kUSBVendorID),v);CFDictionarySetValue(match,CFSTR(kUSBProductID),p);CFRelease(v);CFRelease(p);
 io_iterator_t iter=0; auto ret=IOServiceGetMatchingServices(kIOMainPortDefault,match,&iter);
 if(ret){printf("enumerate: %08x\n",ret);return 1;} auto service=IOIteratorNext(iter);IOObjectRelease(iter);
 if(!service){puts("Audio 2 DJ not connected");return 1;}
 IOCFPlugInInterface** plugin=nullptr;SInt32 score=0;
 ret=IOCreatePlugInInterfaceForService(service,kIOUSBDeviceUserClientTypeID,kIOCFPlugInInterfaceID,&plugin,&score);IOObjectRelease(service);
 if(ret||!plugin){printf("plugin: %08x\n",ret);return 1;}
 IOUSBDeviceInterface** dev=nullptr;auto hr=(*plugin)->QueryInterface(plugin,CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),(LPVOID*)&dev);IODestroyPlugInInterface(plugin);
 if(hr||!dev){puts("device interface unavailable");return 1;}
 ret=(*dev)->USBDeviceOpen(dev);printf("USBDeviceOpen: %08x\n",ret);
 if(ret){(*dev)->Release(dev);return 2;}
 UInt8 config=0;(*dev)->GetConfiguration(dev,&config);printf("configuration=%u\n",config);if(!config){IOUSBConfigurationDescriptorPtr descriptor=nullptr;ret=(*dev)->GetConfigurationDescriptorPtr(dev,0,&descriptor);if(!ret&&descriptor){ret=(*dev)->SetConfiguration(dev,descriptor->bConfigurationValue);printf("SetConfiguration: %08x\n",ret);}}
 IOUSBFindInterfaceRequest request={kIOUSBFindInterfaceDontCare,kIOUSBFindInterfaceDontCare,kIOUSBFindInterfaceDontCare,kIOUSBFindInterfaceDontCare};
 io_iterator_t interfaces=0;ret=(*dev)->CreateInterfaceIterator(dev,&request,&interfaces);
 while(auto item=IOIteratorNext(interfaces)){
 plugin=nullptr;ret=IOCreatePlugInInterfaceForService(item,kIOUSBInterfaceUserClientTypeID,kIOCFPlugInInterfaceID,&plugin,&score);IOObjectRelease(item);if(ret||!plugin)continue;
 IOUSBInterfaceInterface245** iface=nullptr;hr=(*plugin)->QueryInterface(plugin,CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245),(LPVOID*)&iface);IODestroyPlugInInterface(plugin);if(hr||!iface)continue;
 UInt8 number=0,alt=0;(*iface)->GetInterfaceNumber(iface,&number);(*iface)->GetAlternateSetting(iface,&alt);printf("interface=%u alternate=%u\n",number,alt);
 ret=(*iface)->USBInterfaceOpen(iface);printf("USBInterfaceOpen: %08x\n",ret);
 if(!ret){
 (*iface)->SetAlternateInterface(iface,0);ret=(*iface)->SetAlternateInterface(iface,1);printf("SetAlternateInterface(1): %08x\n",ret);
 UInt8 count=0;(*iface)->GetNumEndpoints(iface,&count);UInt8 in=0,out=0;
 for(UInt8 pipe=1;pipe<=count;pipe++){UInt8 direction,endpoint,type,interval;UInt16 size;ret=(*iface)->GetPipeProperties(iface,pipe,&direction,&endpoint,&type,&size,&interval);printf("pipe=%u direction=%u endpoint=%u type=%u max=%u interval=%u status=%08x\n",pipe,direction,endpoint,type,size,interval,ret);if(endpoint==1&&type==kUSBBulk){if(direction==kUSBIn)in=pipe;else out=pipe;}}
 if(in&&out){UInt8 cmd=1,reply[64]={};UInt32 len=sizeof(reply);ret=(*iface)->WritePipe(iface,out,&cmd,1);printf("GET_DEVICE_INFO write: %08x\n",ret);if(!ret){ret=(*iface)->ReadPipeTO(iface,in,reply,&len,1000,1000);printf("reply: status=%08x bytes=%u",ret,(unsigned)len);if(!ret)for(unsigned i=0;i<len;i++)printf(" %02x",reply[i]);puts("");}}
 if(in&&out)play(iface,in,out);
 (*iface)->USBInterfaceClose(iface);}
 (*iface)->Release(iface);
 }
 IOObjectRelease(interfaces);(*dev)->USBDeviceClose(dev);(*dev)->Release(dev);return 0;
}
