// SPDX-License-Identifier: BSD-2-Clause-Patent
// LMIPRO board startup for the pinned original Legacy GOP backend.
// First qualification: genuine init/start/publication and retained GOP mapping.
// CPU/GPU transfer and synthetic refresh are not enabled by this board wrapper.
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSData.h>
#include <pexpert/pexpert.h>
#include <mach/kmod.h>
#include "LmiproDisplayPolicy.h"
#include "LmiproDisplayTrace.h"
extern "C" kmod_info_t g_kernel_kmod_info;
extern "C" void lmipro_display_static_init(void) __asm__("__GLOBAL__sub_I_NativeLegacyDisplay.cpp");
extern "C" IOService *native_legacy_display_allocate(void);
extern "C" kern_return_t OSKextLoadKextWithIdentifier(const char *);
extern "C" int proc_selfpid(void);

static volatile unsigned stage,registered,clients,clientType,guardSlot;
static volatile int result,clientStatus;
static volatile uint64_t mappedBase,mappedBytes,validationCalls,transferCalls;
static IOService *activeDisplay,*activeProvider;

extern "C" unsigned lmipro_display_report(char *out,unsigned capacity)
{
    char methods[512]={};
    lmiproFormatDisplayMethods(methods,sizeof(methods));
    return snprintf(out,capacity,
      "Legacy initStage=%u status=%d registered=%u guard=%u\n"
      "mapping=%llx bytes=%llu clientCalls=%u type=%u status=%x\n"
      "validate=%llu transfers=%llu active=%u; publication-only test\n%s",
      stage,result,registered,guardSlot,(unsigned long long)mappedBase,
      (unsigned long long)mappedBytes,clients,clientType,clientStatus,
      (unsigned long long)validationCalls,(unsigned long long)transferCalls,
      activeDisplay && activeProvider ? 1U:0U,methods);
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
    lmiproRecordDisplayMethod(kind,selector,(uint32_t)status,proc_selfpid(),values[0],values[1],values[2]);
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
extern "C" void native_legacy_client_enter(task_t,uint32_t type) {clients=clients+1;clientType=type;}
extern "C" void native_legacy_client_observed(IOReturn status,IOUserClient *) {clientStatus=(int)status;}
extern "C" void native_legacy_pm_observed(unsigned,IOReturn,unsigned long) {}
extern "C" void native_legacy_swap_stage(unsigned) {}
extern "C" void native_legacy_frame_observed(unsigned,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t) {}
extern "C" void native_legacy_frame_colors_observed(const uint32_t *,const uint64_t *,uint64_t) {}
extern "C" void native_legacy_swap_validation_observed(bool,unsigned,const void *,const void *) {validationCalls=validationCalls+1;}
extern "C" void native_legacy_surface_rejected(uint64_t,uint64_t,uint32_t,uint32_t,uint64_t,uint64_t,uint64_t) {}
// No frame producer or console-ownership transfer is qualified by publication.
extern "C" bool native_legacy_prepare_scanout(void) {return false;}
extern "C" void native_legacy_transfer_observed(IOReturn,const void *,uint64_t) {transferCalls=transferCalls+1;}
extern "C" void native_legacy_copy_timing_observed(uint64_t,uint64_t) {}
extern "C" void native_legacy_retirement_observed(IOService *) {}
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
                    activeDisplay=display;activeProvider=nub;stage=8;result=0;
                    // These initial references own the active services until reboot.
                    ranges->release();memory->release();dt->release();return;
                }
            }
        }
    }
    if(attached)display->detach(nub);
    if(dtAttached)nub->detachFromParent(dt,gIODTPlane);
    if(nubStarted)nub->stop(home);
    if(nubAttached)nub->detach(home);
    OSSafeReleaseNULL(display);OSSafeReleaseNULL(nub);OSSafeReleaseNULL(ranges);
    OSSafeReleaseNULL(memory);OSSafeReleaseNULL(dt);
    panic("LMIPRO DISPLAY init failed stage=%u status=%x registered=%u mapping=%llx bytes=%llu",
        stage,result,registered,(unsigned long long)mappedBase,(unsigned long long)mappedBytes);
}
