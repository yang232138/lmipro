#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdexcept>
// Public API fixture: memory ownership, malformed handoffs and concurrent census.
struct PE_Video {
  // Target LP64 widths, including when this fixture runs on Windows LLP64.
  uint64_t v_baseAddr, v_rowBytes, v_width, v_height, v_depth, v_display;
  char v_pixelFormat[64];
  uint64_t v_offset, v_length;
  unsigned char v_rotate, v_scale;
};
static PE_Video console = {};
static unsigned enabled=0, consoleCalls=0, liveMatches=0, liveIterators=0;
static bool supplied=false, allocMatch=true, allocIterator=true, validIterator=true;
static int entries=0;
static bool targetMatches=true;
struct OSObject {void *metaCast(const char *){return targetMatches?this:nullptr;}};
struct OSDictionary {void release(){assert(liveMatches==1);--liveMatches;}};
struct OSIterator {
  int left;
  OSObject *getNextObject(){static OSObject object;if(left<=0)return nullptr;--left;return &object;}
  bool isValid(){return validIterator;}
  void release(){assert(liveIterators==1);--liveIterators;}
};
struct IOService {
  static OSDictionary *serviceMatching(const char *name) {
    assert(!strcmp(name,"IOService"));
    static OSDictionary match;if(!allocMatch)return nullptr;++liveMatches;return &match;
  }
  static OSIterator *getMatchingServices(OSDictionary *) {
    static OSIterator it;if(!allocIterator)return nullptr;++liveIterators;it.left=entries;return &it;
  }
};
static int PE_current_console(PE_Video *out){++consoleCalls;*out=console;return 0;}
static int PE_parse_boot_argn(const char *name,void *out,int size){assert(!strcmp(name,"lmipro-display-watch")&&size==sizeof(enabled));if(!supplied)return 0;memcpy(out,&enabled,sizeof enabled);return 1;}
[[noreturn]] static void panic(const char *fmt,...){char out[2048];va_list ap;va_start(ap,fmt);vsnprintf(out,sizeof out,fmt,ap);va_end(ap);throw std::runtime_error(out);}
#if defined(LMIPRO_DISPLAY_BOARD) && LMIPRO_DISPLAY_BOARD
using kern_return_t=int;
static void *current_thread();
static kern_return_t thread_terminate(void *);
#endif
#include "../package/LmiproDisplayWatch.h"
#if defined(LMIPRO_DISPLAY_BOARD) && LMIPRO_DISPLAY_BOARD
static unsigned boardReports=0;
static bool framePresented=false;
extern "C" unsigned lmipro_display_frame_presented(){return framePresented;}
static void *current_thread(){return (void *)1;}
static kern_return_t thread_terminate(void *t){assert(t==(void *)1);throw std::runtime_error("watch retired");}

extern "C" unsigned lmipro_ui_exit_report(char *out,unsigned size){return snprintf(out,size,"UI committed exits test fixture\n");}
extern "C" unsigned lmipro_ui_stack_report(char *out,unsigned capacity) {
  return snprintf(out,capacity,"UI entry stacks fixture\n");
}
extern "C" unsigned lmipro_ui_trace_report(char *out,unsigned capacity) {
  return snprintf(out,capacity,"UI snapshot fixture\n");
}
extern "C" unsigned lmipro_display_report(char *out,unsigned capacity) {
  ++boardReports;return snprintf(out,capacity,"board snapshot fixture\n");
}
#endif
int main(){
  lmiproDisplayCheckpoint(600,"1:launchd");assert(!consoleCalls);
  supplied=true;lmiproDisplayCheckpoint(600,"1:launchd");assert(!consoleCalls);
  uint64_t bytes=1;assert(lmiproConsoleLayout(console,&bytes)==1&&bytes==0);
  console.v_baseAddr=0x9c000000;console.v_width=1080;console.v_height=2400;
  console.v_rowBytes=4352;console.v_depth=32;
  assert(lmiproConsoleLayout(console,&bytes)==0&&bytes==10444800);
  PE_Video good=console;
  console.v_rowBytes=4319;assert(lmiproConsoleLayout(console,&bytes)==3&&bytes==0);
  console=good;console.v_depth=24;assert(lmiproConsoleLayout(console,&bytes)==2);
  console=good;console.v_rowBytes=UINT64_MAX;assert(lmiproConsoleLayout(console,&bytes)==3);
  console=good;console.v_baseAddr=UINT64_MAX-7;console.v_offset=8;
  assert(lmiproConsoleLayout(console,&bytes)==4&&bytes==0);
  console=good;console.v_baseAddr=UINT64_MAX-7;assert(lmiproConsoleLayout(console,&bytes)==4);
  assert(lmiproCountDisplayServices("test")==0);
  entries=2;assert(lmiproCountDisplayServices("test")==2);
  entries=1000;assert(lmiproCountDisplayServices("test")==33);
  targetMatches=false;assert(lmiproCountDisplayServices("absent")==0);
  entries=4097;assert(lmiproCountDisplayServices("absent")==-3);
  entries=4096;assert(lmiproCountDisplayServices("absent")==0);
  targetMatches=true;
  validIterator=false;assert(lmiproCountDisplayServices("test")==-2);validIterator=true;
  allocIterator=false;assert(lmiproCountDisplayServices("test")==-1);allocIterator=true;
  allocMatch=false;assert(lmiproCountDisplayServices("test")==-1);allocMatch=true;
  assert(!liveMatches&&!liveIterators);
  console=good;memset(console.v_pixelFormat,'F',64);entries=0;
  char out[576];lmiproFormatDisplaySnapshot(out,sizeof out);
  assert(strstr(out,"status=0 layout=0 base=9c000000"));
  assert(strstr(out,"MobileFB=0 LegacyFB=0 IOSurface=0 GOP=0"));
  assert(console.v_pixelFormat[63]=='F'); // Never modifies PE state.
  char tiny[8];memset(tiny,'X',sizeof tiny);lmiproFormatDisplaySnapshot(tiny,sizeof tiny);assert(tiny[7]==0);
  lmiproFormatDisplaySnapshot(nullptr,0);
  enabled=1;bool caught=false;
  try{lmiproDisplayCheckpoint(600,"1:launchd 57:backboardd");}
  catch(const std::runtime_error &e){caught=true;assert(strstr(e.what(),"LMIPRO UI CHECKPOINT timeout=600"));assert(!strstr(e.what(),"57:backboardd"));assert(strstr(e.what(),"END UI SNAPSHOT"));}
  assert(caught&&!liveMatches&&!liveIterators);
#if defined(LMIPRO_DISPLAY_BOARD) && LMIPRO_DISPLAY_BOARD
  assert(boardReports==1);
  enabled=2;framePresented=false;caught=false;
  try{lmiproDisplayCheckpoint(600,"1:launchd");}catch(const std::runtime_error &e){caught=true;assert(strstr(e.what(),"LMIPRO UI CHECKPOINT"));}
  assert(caught);framePresented=true;caught=false;
  try{lmiproDisplayCheckpoint(600,"1:launchd");}catch(const std::runtime_error &e){caught=true;assert(!strcmp(e.what(),"watch retired"));}
  assert(caught);
#endif
  puts("PASS: disabled path; malformed/overflowing layout; bounded registry counts; allocation/mutation errors; reference balance; bounded strings; no console mutation");
}
