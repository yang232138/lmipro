extern "C" void lmipro_watchdog_static_init(void) {}
static unsigned cases;
static void reset(){
 if(watchdog){watchdog->free();watchdog=nullptr;}services.clear();
 pendingServices.clear();deferredMatching=true;publishFail=foreignMatch=duplicateMatch=false;
 syncRegistrations=asyncRegistrations=0;
 badBase=badClient=preFail=postFail=iteratorInvalid=initFail=attachFail=startFail=timerFail=false;
 privileged=true;loadError=0;bootArg=1;now=NSEC_PER_SEC;originalLock.held=false;interrupts=true;
 memset((void*)lmipro_ram_watchdog_state,0,sizeof(lmipro_ram_watchdog_state));
}
static void poll(){assert(lastTimer&&lastTimer->active);auto *t=lastTimer;t->active=false;t->fn(t->parameter,nullptr);}
int main(){
 reset();bootArg=0;assert(lmipro_ram_watchdog_prepare()==0&&!watchdog&&!lastTimer);cases++;
 reset();loadError=(int)0xdc00800d;assert(lmipro_ram_watchdog_prepare()==ENXIO);
 assert(!strcmp(lmipro_ram_watchdog_step,"load")&&lmipro_ram_watchdog_detail==loadError);cases++;
 reset();badBase=true;assert(lmipro_ram_watchdog_prepare()==ENOEXEC);cases++;
 reset();badClient=true;assert(lmipro_ram_watchdog_prepare()==ENOEXEC);cases++;
 reset();services.push_back(IOService::getResourceService());assert(lmipro_ram_watchdog_prepare()==0&&!watchdog&&!strcmp(lmipro_ram_watchdog_step,"existing"));cases++;
 reset();iteratorInvalid=true;assert(lmipro_ram_watchdog_prepare()==EAGAIN);cases++;
 reset();preFail=true;assert(lmipro_ram_watchdog_prepare()==ENOMEM);cases++;
 reset();postFail=true;assert(lmipro_ram_watchdog_prepare()==ENOEXEC);cases++;
 reset();initFail=true;assert(lmipro_ram_watchdog_prepare()==ENOMEM);cases++;
 reset();timerFail=true;assert(lmipro_ram_watchdog_prepare()==ENOMEM);cases++;
 reset();attachFail=true;assert(lmipro_ram_watchdog_prepare()==ENXIO&&!strcmp(lmipro_ram_watchdog_step,"attach"));cases++;
 reset();startFail=true;assert(lmipro_ram_watchdog_prepare()==ENXIO&&!strcmp(lmipro_ram_watchdog_step,"start"));cases++;
 reset();publishFail=true;assert(lmipro_ram_watchdog_prepare()==ENXIO&&!strcmp(lmipro_ram_watchdog_step,"match")&&lmipro_ram_watchdog_detail==0);cases++;
 reset();foreignMatch=true;assert(lmipro_ram_watchdog_prepare()==ENXIO&&lmipro_ram_watchdog_detail==-ENXIO);cases++;
 reset();duplicateMatch=true;assert(lmipro_ram_watchdog_prepare()==EEXIST&&lmipro_ram_watchdog_detail==2);cases++;
 reset();assert(!lmipro_ram_watchdog_prepare());assert(lmipro_ram_watchdog_state[0]==3&&countServices(watchdog)==1);cases++;
 assert(syncRegistrations==1&&!asyncRegistrations&&pendingServices.empty());cases++;
 assert(!strcmp(lmipro_ram_watchdog_step,"ready")&&lmipro_ram_watchdog_detail==1);cases++;
 assert(lmipro_ram_watchdog_prepare()==EINVAL);cases++;
 IOUserClient *client=nullptr;privileged=false;assert(watchdog->newUserClient(nullptr,nullptr,1,nullptr,&client)==kIOReturnNotPrivileged&&!client);cases++;
 privileged=true;assert(watchdog->newUserClient(nullptr,nullptr,2,nullptr,&client)==kIOReturnBadArgument&&!client);cases++;
 assert(!watchdog->newUserClient(nullptr,nullptr,1,nullptr,&client)&&client==&originalClient&&lmipro_ram_watchdog_state[1]==1);cases++;
 assert(originalOpenCalls>=3);cases++;
 poll();assert(lastTimer->active&&lmipro_ram_watchdog_state[2]==1&&lmipro_ram_watchdog_state[4]==1);cases++;
 setField(watchdog,0xb8,17);setField(watchdog,0xc8,now+90ULL*NSEC_PER_SEC);now+=NSEC_PER_SEC;poll();assert(lmipro_ram_watchdog_state[3]==17);cases++;
 *((char*)watchdog+0xa8)=0;now+=1000ULL*NSEC_PER_SEC;poll();assert(lastTimer->active&&lmipro_ram_watchdog_state[4]==0);cases++;
 *((char*)watchdog+0xa8)=1;setField(watchdog,0xc8,now+90ULL*NSEC_PER_SEC);poll();assert(lastTimer->active);cases++;
 now+=91ULL*NSEC_PER_SEC;bool caught=false;try{poll();}catch(const Panic &p){caught=strstr(p.what(),"software deadline expired");}assert(caught&&!originalLock.held&&interrupts);cases++;
 reset();assert(!lmipro_ram_watchdog_prepare());setField(watchdog,0xc8,0);caught=false;try{poll();}catch(const Panic&){caught=true;}assert(caught);cases++;
 reset();assert(!lmipro_ram_watchdog_prepare());watchdog->stop(IOService::getResourceService());assert(!lastTimer->active&&lmipro_ram_watchdog_state[7]==1);cases++;
 reset();assert(!lastTimer);cases++;
 reset();deferredMatching=false;assert(!lmipro_ram_watchdog_prepare()&&syncRegistrations==1);cases++;
 reset();assert(!lmipro_ram_watchdog_prepare());services.clear();watchdog->registerService(kIOServiceAsynchronous);
 assert(syncRegistrations==2&&!asyncRegistrations&&pendingServices.empty()&&countServices(watchdog)==1);cases++;
 reset();
 printf("PASS %u actual C++ watchdog lifecycle/deadline/refusal cases; original APIs explicitly mocked\n",cases);
}
