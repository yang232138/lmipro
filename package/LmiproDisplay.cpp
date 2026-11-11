// SPDX-License-Identifier: BSD-2-Clause-Patent
// LMIPRO board startup for the pinned original Legacy GOP backend.
// First qualification: genuine init/start/publication and retained GOP mapping.
// Original CPU presentation is enabled explicitly; GPU and synthetic refresh stay off.
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSIterator.h>
#include <machine/machine_routines.h>
#include "LmiproScanoutPolicy.h"
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSData.h>
#include <pexpert/pexpert.h>
#include <mach/kmod.h>
#include <kern/thread.h>
#include <kern/backtrace.h>
#include <kern/sched_prim.h>
#include <sys/errno.h>
#include "LmiproDisplayPolicy.h"
#include "LmiproModePolicy.h"
#include "LmiproDisplayTrace.h"
extern "C" kmod_info_t g_kernel_kmod_info;
extern "C" void lmipro_display_static_init(void) __asm__("__GLOBAL__sub_I_NativeLegacyDisplay.cpp");
extern "C" IOService *native_legacy_display_allocate(void);
extern "C" kern_return_t OSKextLoadKextWithIdentifier(const char *);
extern "C" int proc_selfpid(void);
extern "C" void proc_selfname(char *,int);
extern "C" void lmipro_ui_trace_enable(unsigned);
extern "C" void lmipro_ui_stacks_enable(unsigned);
extern "C" volatile int lmipro_data_owner_stage,lmipro_data_owner_status;

static volatile unsigned stage,registered,clients,clientType,guardSlot;
static volatile int result,clientStatus;
static volatile uint64_t mappedBase,mappedBytes,validationCalls,transferCalls;
static IOService *activeDisplay,*activeProvider;

static unsigned uiStacksEnabled;
static volatile unsigned uiStackGate=1;
static volatile uint64_t uiStackChecks;
static uint64_t uiStacksStarted;
extern "C" __attribute__((noinline)) unsigned lmipro_ui_stack_ready(void) {
    __atomic_fetch_add(&uiStackChecks,1,__ATOMIC_RELAXED);
    if(!uiStacksEnabled){uiStackGate=1;return 0;}
    if(ml_at_interrupt_context()){uiStackGate=2;return 0;}
    if(!ml_get_interrupts_enabled()){uiStackGate=3;return 0;}
    if(!preemption_enabled()){uiStackGate=4;return 0;}
    uint64_t ns=0;absolutetime_to_nanoseconds(mach_absolute_time()-uiStacksStarted,&ns);
    if(ns<UINT64_C(30000000000)){uiStackGate=5;return 0;}
    uiStackGate=0;return 1;
}
extern "C" __attribute__((noinline)) unsigned lmipro_ui_stack_capture(uintptr_t *frames,unsigned capacity,int *error,unsigned *flags) {
    if(!frames || !error || !flags || !capacity || capacity>8)return 0;
    *error=0;*flags=0;
    if(!lmipro_ui_stack_ready()){*error=EDEADLK;return 0;}
    // Exact pinned XNU current-thread API, using its default validated copyin.
    // No foreign task, manually followed user pointer or private object layout.
    struct backtrace_user_info info={};
    unsigned count=backtrace_user(frames,capacity,nullptr,&info);
    *error=info.btui_error;*flags=(unsigned)info.btui_info;return count;
}


static volatile unsigned cpuEnabled,scanoutOwned,handoffBusy,pmStage;
static volatile int pmStatus,transferStatus,handoffStatus;
static volatile uint64_t successfulCopies,copiedBytes,failedCopies,retirements,surfaceFrames;
static unsigned currentFrameSurface;
static volatile unsigned validationAccepted,validationReason;
extern "C" uint32_t native_legacy_surface_format(OSObject *);
extern "C" uint32_t native_legacy_surface_planes(OSObject *);
extern "C" IOMemoryDescriptor *lmipro_original_surface_descriptor(OSObject *);
extern "C" __attribute__((noinline)) IOMemoryDescriptor *native_legacy_surface_descriptor(OSObject *surface) {
    if(!cpuEnabled || !surface || !surface->metaCast("IOSurface") ||
       !lmiproCPUFormat(native_legacy_surface_format(surface),native_legacy_surface_planes(surface)))return nullptr;
    return lmipro_original_surface_descriptor(surface);
}
static __attribute__((noinline)) bool lmiproNoGPUProducer(void) {
    OSDictionary *match=IOService::serviceMatching("IOService");if(!match)return false;
    OSIterator *it=IOService::getMatchingServices(match);match->release();if(!it)return false;
    unsigned seen=0;bool allowed=true;OSObject *object;
    while((object=it->getNextObject())) {
        if(++seen>4096 || object->metaCast("IOAcceleratorES") ||
           object->metaCast("IOAccelerator") || object->metaCast("IOGPU")) {allowed=false;break;}
    }
    if(!it->isValid() || !seen)allowed=false;
    it->release();return allowed;
}
extern "C" __attribute__((noinline)) unsigned lmipro_display_frame_presented(void) {
    return lmiproKeepPresentedFrame(cpuEnabled,__atomic_load_n(&scanoutOwned,__ATOMIC_ACQUIRE),
      __atomic_load_n(&successfulCopies,__ATOMIC_ACQUIRE),__atomic_load_n(&copiedBytes,__ATOMIC_ACQUIRE),
      __atomic_load_n(&failedCopies,__ATOMIC_ACQUIRE),__atomic_load_n(&retirements,__ATOMIC_ACQUIRE),
      __atomic_load_n(&surfaceFrames,__ATOMIC_ACQUIRE));
}


static LmiproModePolicy modePolicy;
static IORegistryEntry *modeChosen;
static void lmiproModeFinish(bool keep) {
    if(modeChosen && !keep) {
        if(modePolicy.rotationAdded)modeChosen->removeProperty("display-rotation");
        if(modePolicy.scaleAdded)modeChosen->removeProperty("display-scale");
    }
    OSSafeReleaseNULL(modeChosen);
}
static __attribute__((noinline)) bool lmiproConfigureDisplayMode(const PE_Video &video) {
    unsigned enabled=0;PE_parse_boot_argn("lmipro-display-mode",&enabled,sizeof(enabled));
    if(!enabled)return true;
    if(enabled!=1 || video.v_scale!=1 || video.v_rotate || video.v_display!=1) {
        modePolicy.status=-1;return false;
    }
    modeChosen=IORegistryEntry::fromPath("/chosen",gIODTPlane);
    if(!modeChosen){modePolicy.status=-5;return false;}
    auto read=[](const char *key,uint32_t *word)->int {
        OSObject *object=modeChosen->getProperty(key);if(!object)return 0;
        OSData *data=OSDynamicCast(OSData,object);
        if(!data || data->getLength()!=4)return -1;
        memcpy(word,data->getBytesNoCopy(),4);return 1;
    };
    auto write=[](const char *key,unsigned word)->bool {
        OSData *data=OSData::withBytes(&word,4);
        bool ok=data && modeChosen->setProperty(key,data);OSSafeReleaseNULL(data);return ok;
    };
    auto erase=[](const char *key){modeChosen->removeProperty(key);};
    if(!lmiproModeInstall(modePolicy,video.v_scale,video.v_rotate,read,write,erase)) {
        lmiproModeFinish(false);return false;
    }
    return true;
}


extern "C" unsigned lmipro_display_report(char *out,unsigned capacity)
{
    char methods[1200]={},opens[640]={};
    lmiproFormatDisplayMethods(methods,sizeof(methods));
    lmiproFormatClients(opens,sizeof(opens));
    return snprintf(out,capacity,
      "Legacy initStage=%u status=%d registered=%u guard=%u\n"
      "mapping=%llx bytes=%llu clientCalls=%u type=%u status=%x\n"
      "validate=%llu transfers=%llu active=%u; CPU presentation candidate\n"
      "Mode policy stage=%u status=%d scale=1 rotation=0\n"
      "Data owner stage=%d status=%d\n"
      "UI stack context gate=%u checks=%llu (0 ready,1 off,2 irq,3 irqOff,4 preempt,5 warm)\n"
      "CPU enabled=%u owned=%u handoff=%d PM=%u/%x accepted=%u reason=%u\n"
      "copied=%llu bytes=%llu failed=%llu retired=%llu surfaces=%llu last=%x\n%s%s",
      stage,result,registered,guardSlot,(unsigned long long)mappedBase,
      (unsigned long long)mappedBytes,clients,clientType,clientStatus,
      (unsigned long long)validationCalls,(unsigned long long)transferCalls,
      activeDisplay && activeProvider ? 1U:0U,modePolicy.stage,modePolicy.status,
      lmipro_data_owner_stage,lmipro_data_owner_status,
      uiStackGate,(unsigned long long)__atomic_load_n(&uiStackChecks,__ATOMIC_RELAXED),
      cpuEnabled,scanoutOwned,handoffStatus,pmStage,pmStatus,validationAccepted,validationReason,
      (unsigned long long)successfulCopies,(unsigned long long)copiedBytes,
      (unsigned long long)failedCopies,(unsigned long long)retirements,(unsigned long long)surfaceFrames,transferStatus,methods,opens);
}
extern "C" void lmipro_display_method_event(IOUserClient *client,uint32_t selector,
    IOExternalMethodArguments *args,unsigned kind,IOReturn status)
{
    if(!gLmiproDisplayTraceEnabled || !client ||
       !client->metaCast("IOMobileFramebufferUserClient"))return;
    uint64_t values[3]={};
    // The MIG dispatch supplies a kernel-owned scalar array. Copy at most three
    // input values on entry only; retain no pointers and inspect no user memory.
    if(kind==0 && args && args->scalarInput) {
        unsigned count=args->scalarInputCount<3?args->scalarInputCount:3;
        for(unsigned i=0;i<count;++i)values[i]=args->scalarInput[i];
    }
    unsigned outputCount=0;
    // MIG supplies kernel scalar storage. Read only successful replies, at most
    // three words; never touch structureOutput or any user address/descriptor.
    if(kind==1 && status==kIOReturnSuccess && args && args->scalarOutput) {
        outputCount=args->scalarOutputCount;
        unsigned count=outputCount<3?outputCount:3;
        for(unsigned i=0;i<count;++i)values[i]=args->scalarOutput[i];
    }
    char name[17]={};proc_selfname(name,sizeof(name));name[16]=0;
    lmiproRecordDisplayMethod(kind,selector,(uint32_t)status,proc_selfpid(),values[0],values[1],values[2],name,outputCount);
}
extern "C" __attribute__((noreturn)) void native_legacy_backend_guard(unsigned slot,void *,void *caller)
{
    guardSlot=slot;
    panic("LMIPRO DISPLAY backend guard slot=%u caller=%p initStage=%u status=%d",slot,caller,stage,result);
}
extern "C" bool native_legacy_gop_mapping_observed(IOService *,IOMemoryMap *mapping,uint32_t width,uint32_t height)
{
    if(!mapping || width!=1080 || height!=2400 || !mapping->getVirtualAddress() ||
       mapping->getPhysicalAddress()!=0x9c000000ULL || mapping->getLength()!=10368000)return false;
    mappedBase=mapping->getPhysicalAddress();mappedBytes=mapping->getLength();return true;
}
extern "C" void native_legacy_registration_observed(IOService *) {registered=registered+1;}
extern "C" void native_legacy_client_enter(task_t,uint32_t type) {
    clients=clients+1;clientType=type;
    char name[17]={};proc_selfname(name,sizeof(name));name[16]=0;
    lmiproClientEvent(0,thread_tid(current_thread()),type,proc_selfpid(),name,0);
}
extern "C" void native_legacy_client_observed(IOReturn status,IOUserClient *) {
    clientStatus=(int)status;
    lmiproClientEvent(1,thread_tid(current_thread()),0,0,nullptr,(uint32_t)status);
}
extern "C" void native_legacy_pm_observed(unsigned value,IOReturn status,unsigned long) {pmStage=value;pmStatus=(int)status;}
extern "C" void native_legacy_swap_stage(unsigned) {}
extern "C" void native_legacy_frame_observed(unsigned blank,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t) {currentFrameSurface=!blank;}
extern "C" void native_legacy_frame_colors_observed(const uint32_t *,const uint64_t *,uint64_t) {}
extern "C" void native_legacy_swap_validation_observed(bool accepted,unsigned reason,const void *,const void *) {validationCalls=validationCalls+1;validationAccepted=accepted;validationReason=reason;}
extern "C" void native_legacy_surface_rejected(uint64_t,uint64_t,uint32_t,uint32_t,uint64_t,uint64_t,uint64_t) {}
// Only a validated original CPU surface reaches this callback. BOOT_PROGRESS
// is off in this image; the official entry/panic renderer writes only at entry
// or an actual panic. Release the original video console before any copy.
extern "C" __attribute__((noinline)) bool native_legacy_prepare_scanout(void) {
    if(!cpuEnabled)return false;
    if(__atomic_load_n(&scanoutOwned,__ATOMIC_ACQUIRE))return true;
    if(ml_at_interrupt_context() || !ml_get_interrupts_enabled())return false;
    if(!__sync_bool_compare_and_swap(&handoffBusy,0,1))return false;
    PE_Video video={};uint64_t bytes=0;bool ok=false;
    handoffStatus=PE_current_console(&video);
    if(!handoffStatus && lmiproDisplayHandoff(video.v_baseAddr,video.v_width,video.v_height,
       video.v_depth,video.v_rowBytes,video.v_offset,video.v_rotate,&bytes) &&
       video.v_display==1 && mappedBase==video.v_baseAddr && mappedBytes==bytes && lmiproNoGPUProducer()) {
        handoffStatus=PE_initialize_console(&video,kPEReleaseScreen);
        ok=handoffStatus==0;
        if(ok)__atomic_store_n(&scanoutOwned,1,__ATOMIC_RELEASE);
    } else handoffStatus=(int)kIOReturnNotReady;
    __sync_lock_release(&handoffBusy);return ok;
}
extern "C" void native_legacy_transfer_observed(IOReturn status,const void *,uint64_t bytes) {
    transferCalls=transferCalls+1;transferStatus=(int)status;
    if(status==kIOReturnSuccess && bytes) {
        __atomic_fetch_add(&copiedBytes,bytes,__ATOMIC_RELEASE);
        __atomic_fetch_add(&successfulCopies,1,__ATOMIC_RELEASE);
        if(currentFrameSurface)__atomic_fetch_add(&surfaceFrames,1,__ATOMIC_RELEASE);
    } else __atomic_fetch_add(&failedCopies,1,__ATOMIC_RELEASE);
    currentFrameSurface=0;
}
extern "C" void native_legacy_copy_timing_observed(uint64_t,uint64_t) {}
extern "C" void native_legacy_retirement_observed(IOService *) {__atomic_fetch_add(&retirements,1,__ATOMIC_RELEASE);}
extern "C" bool native_legacy_gpu_offer(void *,IOMemoryDescriptor *,uint64_t,uint32_t,uint32_t) {return false;}
extern "C" void native_legacy_gpu_fallback_observed(int) {}
extern "C" void native_legacy_vsync_observed(unsigned,unsigned) {}

// The normal component-load hook may call early; creation belongs after IOBSD.
extern "C" void native_legacy_display_probe(void) {}
extern "C" void native_legacy_display_late_probe(void)
{
    unsigned enabled=0;
    if(!PE_parse_boot_argn("lmipro-display-init",&enabled,sizeof(enabled)) || enabled!=1 || stage)return;
    unsigned trace=0;PE_parse_boot_argn("lmipro-display-calls",&trace,sizeof(trace));
    gLmiproDisplayTraceEnabled=trace==1;
    unsigned ui=0;PE_parse_boot_argn("lmipro-ui-watch",&ui,sizeof(ui));
    lmipro_ui_trace_enable(ui);
    unsigned stacks=0;PE_parse_boot_argn("lmipro-ui-stacks",&stacks,sizeof(stacks));
    uiStacksEnabled=stacks==1;uiStacksStarted=mach_absolute_time();lmipro_ui_stacks_enable(stacks);
    unsigned cpu=0;PE_parse_boot_argn("lmipro-display-cpu",&cpu,sizeof(cpu));cpuEnabled=cpu==1;
    stage=1;
    result=OSKextLoadKextWithIdentifier("com.apple.iokit.IOMobileGraphicsFamily");
    if(result)panic("LMIPRO DISPLAY family load failed stage=%u status=%d",stage,result);
    stage=2;
    const OSSymbol *className=OSSymbol::withCStringNoCopy("IOMobileFramebufferLegacy");
    const OSMetaClass *parentMeta=className?OSMetaClass::getMetaClassWithName(className):nullptr;
    OSSafeReleaseNULL(className);
    if(!parentMeta || parentMeta->getClassSize()!=0x18a0)
        panic("LMIPRO DISPLAY original class size mismatch stage=%u size=%u",stage,parentMeta?parentMeta->getClassSize():0);
    void *handle=OSMetaClass::preModLoad(g_kernel_kmod_info.name);
    if(!handle)panic("LMIPRO DISPLAY preModLoad failed stage=%u",stage);
    lmipro_display_static_init();
    result=OSMetaClass::postModLoad(handle);
    if(result)panic("LMIPRO DISPLAY postModLoad failed stage=%u status=%d",stage,result);
    stage=3;
    result=OSKextLoadKextWithIdentifier("com.apple.iokit.IOSurface");
    if(result)panic("LMIPRO DISPLAY IOSurface load failed stage=%u status=%d",stage,result);
    OSDictionary *matching=IOService::nameMatching("IOCoreSurfaceRoot");
    IOService *surface=matching?IOService::waitForMatchingService(matching,5000000000ULL):nullptr;
    OSSafeReleaseNULL(matching);
    if(!surface)panic("LMIPRO DISPLAY IOSurface unavailable stage=%u",stage);
    surface->release();
    stage=4;PE_Video video={};uint64_t bytes=0;
    result=PE_current_console(&video);
    if(result || !lmiproDisplayHandoff(video.v_baseAddr,video.v_width,video.v_height,
        video.v_depth,video.v_rowBytes,video.v_offset,video.v_rotate,&bytes) || video.v_display!=1 ||
        strncmp(video.v_pixelFormat,"BBBBBBBBGGGGGGGGRRRRRRRR",24))
        panic("LMIPRO DISPLAY unexpected GOP handoff stage=%u status=%d base=%llx",stage,result,(unsigned long long)video.v_baseAddr);
    if(!lmiproConfigureDisplayMode(video))
        panic("LMIPRO DISPLAY mode policy failed stage=%u status=%d",modePolicy.stage,modePolicy.status);
    IOService *display=native_legacy_display_allocate();
    IOPlatformDevice *nub=new IOPlatformDevice;
    IOService *home=IOService::getResourceService();
    IODeviceMemory *memory=IODeviceMemory::withRange(video.v_baseAddr,bytes);
    OSArray *ranges=OSArray::withCapacity(1);
    IORegistryEntry *dt=IORegistryEntry::fromPath("/arm-io",gIODTPlane);
    bool nubAttached=false,nubStarted=false,dtAttached=false,attached=false;
    result=(int)kIOReturnNoMemory;
    if(display && nub && home && memory && ranges && dt &&
       display->metaCast("IOMobileFramebufferLegacy") && display->metaCast("IOMobileFramebuffer") &&
       display->init() && nub->init() && ranges->setObject(memory)) {
        stage=5;nub->setDeviceMemory(ranges);nub->setName("NativeGOPFramebuffer");
        nubAttached=nub->attach(home);if(nubAttached)nubStarted=nub->start(home);
        if(nubStarted) {
            const char type[]="display-subsystem";
            OSData *data=OSData::withBytes(type,sizeof(type));
            if(data && nub->setProperty("device_type",data)) {
                nub->setName("gop-display",gIODTPlane);dtAttached=nub->attachToParent(dt,gIODTPlane);
            }
            OSSafeReleaseNULL(data);
            SInt32 score=0;stage=6;result=(int)kIOReturnNoDevice;
            if(dtAttached && display->probe(nub,&score)==display)attached=display->attach(nub);
            if(attached) {
                stage=7;result=(int)kIOReturnNotReady;
                if(display->start(nub)) {
                    activeDisplay=display;activeProvider=nub;stage=8;result=0;lmiproModeFinish(true);
                    // These initial references own the active services until reboot.
                    ranges->release();memory->release();dt->release();return;
                }
            }
        }
    }
    lmiproModeFinish(false);
    if(attached)display->detach(nub);
    if(dtAttached)nub->detachFromParent(dt,gIODTPlane);
    if(nubStarted)nub->stop(home);
    if(nubAttached)nub->detach(home);
    OSSafeReleaseNULL(display);OSSafeReleaseNULL(nub);OSSafeReleaseNULL(ranges);
    OSSafeReleaseNULL(memory);OSSafeReleaseNULL(dt);
    panic("LMIPRO DISPLAY init failed stage=%u status=%x registered=%u mapping=%llx bytes=%llu",
        stage,result,registered,(unsigned long long)mappedBase,(unsigned long long)mappedBytes);
}
