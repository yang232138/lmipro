"""Host-check the exact one-shot kernel source using a narrow XNU fixture."""
import os
from pathlib import Path
import subprocess
import tempfile

PATCH = Path(__file__).resolve().parents[1] / 'patches/xnu/lmipro-bootstrap-probe.patch'
PROBE = PATCH.read_text().split('+++ b/osfmk/arm64/lmipro_bootstrap_probe.h\n', 1)[1].split('\n', 1)[1]
HEADER = '\n'.join(line[1:] for line in PROBE.splitlines() if line.startswith('+')) + '\n'
FIXTURE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define ARM_DBG_CR_BYTE_ADDRESS_SELECT_MASK 0x1e0u
#define ARM_DBG_CR_ENABLE_ENABLE 1u
#define ARM_DEBUG_STATE64 15
#define ARM_DEBUG_STATE64_COUNT 130
#define MACH_VM_MAX_ADDRESS 0x800000000000ULL
#define MACH_PORT_NULL 0u
#define MACH_PORT_VALID(x) ((x) != 0 && (x) != UINT32_MAX)
#define MACH_PORT_TYPE_SEND 0x10000
#define KERN_SUCCESS 0
#define THREAD_BASIC_INFO 3
#define THREAD_BASIC_INFO_COUNT 10
#define THREAD_TAG_MAINTHREAD 1
#define P_LTRACED 1
#define P_LNOATTACH 2
#define THREAD_NULL NULL

typedef int kern_return_t;
typedef uint64_t user_addr_t;
typedef uint32_t mach_port_name_t, mach_port_type_t, mach_msg_type_number_t;
typedef void *thread_state_t, *thread_info_t;
typedef struct {int run_state;} thread_basic_info_data_t;
typedef struct {uint64_t bvr[16], bcr[16];} arm_debug_state64_t;
typedef struct {struct {int flavor;} dsh; struct {arm_debug_state64_t ds64;} uds;} arm_debug_state_t;
typedef struct {uint64_t pc, lr, sp, fp, x[29];} arm_saved_state_t;
struct thread {struct {arm_debug_state_t *DebugData;} machine;};
typedef struct thread *thread_t;
struct proc {unsigned p_lflag;};
typedef struct proc *proc_t;
struct task {uint32_t task_shared_region_slide; uint8_t task_shared_region_uuid[16]; uint64_t mach_header_vm_address; void *itk_space;};
typedef struct task *task_t;
static struct task task;
static struct thread th;
static struct proc proc;
static arm_debug_state_t deb;
static bool enabled, bad_word, bad_right;
static unsigned from_seconds, seconds, reads, name_reads, right_checks;
static uint64_t tick;
static uint32_t fixture_port;
static uint8_t user_stack[0x300];
static struct {int num_breakpoint_pairs;} caps = {2};
#define arm_debug_info() (&caps)
static unsigned PE_parse_boot_argn(const char *name,void *dst,unsigned n) {
    assert(!strcmp(name,"lmipro-bootstrap-at") && n==sizeof(unsigned));
    *(unsigned *)dst=from_seconds; return enabled;
}
static void proc_selfname(char *out,int n) {snprintf(out,n,"SpringBoard");}
static int proc_selfpid(void) {return 27;}
static uint64_t thread_tid(thread_t t) {return t==&th ? 295 : 296;}
static unsigned thread_get_tag(thread_t t) {return t==&th ? THREAD_TAG_MAINTHREAD : 0;}
static task_t current_task(void) {return &task;}
static unsigned ml_get_interrupts_enabled(void) {return 1;}
static unsigned preemption_enabled(void) {return 1;}
static uint64_t mach_absolute_time(void) {return tick+=1000000;}
static void absolutetime_to_nanoseconds(uint64_t t,uint64_t *o) {*o=t;}
uint64_t lmipro_ui_stack_seconds(void) {return seconds;}
static uint64_t get_saved_state_pc(const arm_saved_state_t *s) {return s->pc;}
static uint64_t get_saved_state_sp(const arm_saved_state_t *s) {return s->sp;}
static uint64_t get_saved_state_lr(const arm_saved_state_t *s) {return s->lr;}
static uint64_t get_saved_state_fp(const arm_saved_state_t *s) {return s->fp;}
static const arm_saved_state_t *const_saved_state64(const arm_saved_state_t *s) {return s;}
static int copyin(user_addr_t a,void *out,size_t n) {
    if(a>=0x8d00 && a-0x8d00<sizeof(user_stack) && n<=sizeof(user_stack)-(a-0x8d00)) {
        memcpy(out,user_stack+(a-0x8d00),n);return 0;
    }
    a-=task.task_shared_region_slide;
    uint32_t op=0;
    if(a==0x1dd67d1c4ULL)op=0xd100c3ff;
    else if(a==0x1dd67cb70ULL)op=0xb90002f9;
    else if(a==0x1dd67d214ULL)op=0xd65f03c0;
    else if(a==0x1dd6699d0ULL)op=0xa9045ff8;
    else if(a==0x1dd6699d8ULL)op=0xa9064ff4;
    else if(a+task.task_shared_region_slide==0x5000) {op=fixture_port;++reads;}
    else return EFAULT;
    if(n!=4)return EFAULT;
    if(bad_word && a==0x1dd67cb70ULL)return EFAULT;
    memcpy(out,&op,n);return 0;
}
static int copyinstr(user_addr_t a,void *out,size_t n,size_t *done) {
    ++name_reads;
    if(a==0x7000)return ENAMETOOLONG;
    if(a!=0x6000)return EFAULT;
    const char name[]="com.apple.fixture";assert(sizeof(name)<=n);
    memcpy(out,name,sizeof(name));*done=sizeof(name);return 0;
}
static int machine_thread_set_state(thread_t t,int f,void *s,unsigned n) {
    assert(t==&th && f==ARM_DEBUG_STATE64 && n==ARM_DEBUG_STATE64_COUNT);
    deb.dsh.flavor=f;deb.uds.ds64=*(arm_debug_state64_t *)s;
    t->machine.DebugData=deb.uds.ds64.bvr[0]?&deb:NULL;return 0;
}
static int mach_port_type(void *space,mach_port_name_t name,mach_port_type_t *type) {
    (void)space;assert(name==fixture_port);++right_checks;*type=bad_right?0:MACH_PORT_TYPE_SEND;return 0;
}
bool lmipro_bootstrap_debug_allowed(void) {return !(proc.p_lflag&(P_LTRACED|P_LNOATTACH));}
thread_t lmipro_bootstrap_report_thread(int pid,uint64_t id) {assert(pid==27 && id==295);return &th;}
static void thread_deallocate(thread_t t) {assert(t==&th);}
static int thread_info_internal(thread_t t,int f,void *info,mach_msg_type_number_t *n) {
    assert(t==&th && f==THREAD_BASIC_INFO && *n==THREAD_BASIC_INFO_COUNT);
    ((thread_basic_info_data_t *)info)->run_state=3;return 0;
}
#include "probe.h"
static arm_saved_state_t start(void) {
    arm_saved_state_t ss={.sp=0x9000,.lr=0x100009008};
    ss.x[1]=0x6000;ss.x[2]=0x5000;
    lmb_live=(struct lmb_record){0};lmb_snapshot=lmb_live;lmb_owner=NULL;
    lmb_config=lmb_guard=lmb_lost=0;th.machine.DebugData=NULL;
    task.task_shared_region_slide=0x4d04000;task.mach_header_vm_address=0x100000000;
    memcpy(task.task_shared_region_uuid,lmb_uuid,sizeof(lmb_uuid));
    proc.p_lflag=0;from_seconds=0;seconds=40;enabled=true;bad_word=bad_right=false;
    reads=name_reads=right_checks=0;fixture_port=0x507;
    memset(user_stack,0,sizeof(user_stack));
    return ss;
}
static void store(arm_saved_state_t ss) {
    ss.pc=LMB_STORE+lmb_live.slide;ss.sp-=0x90;
    ss.x[23]=ss.x[2];ss.x[25]=fixture_port;
    assert(lmipro_bootstrap_break(&th,&ss));assert(lmb_live.stored==1);assert(!reads);
}
static void finish(arm_saved_state_t ss,unsigned status) {
    ss.pc=LMB_RET+lmb_live.slide;ss.x[0]=status;
    assert(lmipro_bootstrap_break(&th,&ss));assert(th.machine.DebugData==NULL);
}
static void stack_word(uint64_t address,uint64_t word) {
    assert(address>=0x8d00 && address+8<=0x9000);
    memcpy(user_stack+address-0x8d00,&word,8);
}
static arm_saved_state_t in_flight(arm_saved_state_t wrapper,bool wrong_chain) {
    const uint64_t frame[]={0x8e00,0x8e30,0x8e60,0x8e90,0x8ec0,0x8f60,0x8fc0,0x8ff0};
    const uint64_t ret[]={0x1cb7841b0,0x1cb783fb8,0x1dd68b7ec,0x1dd68ac9c,0x1dd669a90,0x1dd67cb20,0x1dd67d1f4};
    for(unsigned i=0;i<7;++i) {
        stack_word(frame[i],frame[i+1]);
        stack_word(frame[i]+8,ret[i]+task.task_shared_region_slide+((i==4&&wrong_chain)?4:0));
    }
    stack_word(frame[5]-40,wrapper.x[2]);
    stack_word(frame[5]-8,wrapper.x[1]);
    stack_word(frame[7]+8,wrapper.lr);
    wrapper.fp=frame[0];wrapper.sp=0x8db0;wrapper.pc=LMB_MACH2+task.task_shared_region_slide;
    return wrapper;
}
static void enter(arm_saved_state_t ss) {
    arm_saved_state_t ipc=in_flight(ss,false);
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_ENTERED);
}
int main(void) {
    char buf[416];
    arm_saved_state_t ss=start(), ipc=in_flight(ss,false);enabled=false;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(!th.machine.DebugData && !lmipro_bootstrap_report(buf,sizeof(buf)));
    ss=start();ipc=in_flight(ss,false);from_seconds=300;seconds=299;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_IDLE && !th.machine.DebugData);
    ss=start();ipc=in_flight(ss,false);ipc.pc+=4;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_IDLE && !th.machine.DebugData);
    ss=start();ipc=in_flight(ss,false);proc.p_lflag=P_LNOATTACH;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_REFUSED && lmb_live.diagnostic==EPERM && !th.machine.DebugData);
    ss=start();ipc=in_flight(ss,false);bad_word=true;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_REFUSED && lmb_live.diagnostic==ENOEXEC && !th.machine.DebugData);
    ss=start();ipc=in_flight(ss,false);th.machine.DebugData=&deb;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_REFUSED && th.machine.DebugData==&deb);
    ss=start();enter(ss);assert(!strcmp(lmb_live.name,"com.apple.fixture"));
    ipc=in_flight(ss,false);ipc.x[0]=0;
    lmipro_bootstrap_report(buf,sizeof(buf));assert(strstr(buf,"run=3 mach=1/0/0"));
    lmipro_bootstrap_svc(&th,&ipc,-47,true);finish(ss,1102);
    lmipro_bootstrap_report(buf,sizeof(buf));assert(strstr(buf,"ret1/1102 store0 port0/0"));
    assert(!reads && !right_checks && lmb_live.mach_out);
    ss=start();enter(ss);store(ss);finish(ss,0);
    assert(lmb_live.phase==LMB_RETURNED && lmb_live.result==0 && lmb_live.port_check==2 && lmb_live.port==fixture_port);
    assert(reads==1 && right_checks==1 && name_reads==1);
    ipc=in_flight(ss,false);lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_RETURNED && !th.machine.DebugData && name_reads==1);
    ss=start();enter(ss);store(ss);finish(ss,1102);assert(!reads && !right_checks && !lmb_live.port);
    ss=start();enter(ss);store(ss);bad_right=true;finish(ss,0);
    assert(lmb_live.port_check==1 && lmb_live.port==0);
    ss=start();ss.x[1]=0x7000;enter(ss);assert(lmb_live.name_error==ENAMETOOLONG && !lmb_live.name[0]);
    finish(ss,0);assert(lmb_live.phase==LMB_RETURNED && !reads && !lmb_live.port);
    ss=start();enter(ss);ss.sp-=16;finish(ss,0);assert(lmb_live.phase==LMB_MISMATCH && !reads);
    ss=start();ipc=in_flight(ss,true);
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_SCANNING && lmb_live.chain_checked==1 && lmb_live.chain_depth==4);
    assert(!th.machine.DebugData && !name_reads);
    ipc=in_flight(ss,false);seconds=309;
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_ENTERED && lmb_live.chain_checked==2 && lmb_live.chain_depth==7);
    assert(lmb_live.ready_seconds==40 && lmb_live.at_seconds==309 && lmb_live.mach_in && lmb_live.caller==ss.lr);
    assert(lmb_sp==ss.sp && lmb_out==ss.x[2] && !strcmp(lmb_live.name,"com.apple.fixture"));
    lmipro_bootstrap_svc(&th,&ipc,-47,true);
    store(ss);finish(ss,0);
    assert(lmb_live.phase==LMB_RETURNED && lmb_live.mach_out && lmb_live.port_check==2);
    lmipro_bootstrap_report(buf,sizeof(buf));assert(strstr(buf,"ready=40 checks=2 depth=7"));
    ss=start();ipc=in_flight(ss,false);
    lmipro_bootstrap_svc(&th,&ipc,-47,false);
    assert(lmb_live.phase==LMB_ENTERED && lmb_live.chain_checked==1 && lmb_live.at_seconds==40);
    puts("PASS: exact XNU probe body: opt-in/ABI gates, untouched debugger, Mach pending vs wrapper failure, observed output store, real send right and pairing");
}
'''
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root / 'probe.h').write_text(HEADER)
    source = root / 'fixture.c'
    source.write_text(FIXTURE)
    exe = root / ('probe.exe' if os.name == 'nt' else 'probe')
    subprocess.run([os.environ.get('CC', 'clang'), '-std=gnu17', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
