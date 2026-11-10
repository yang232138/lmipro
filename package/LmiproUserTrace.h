// Opt-in PID 1 evidence only: no allocation, user-memory access or register writes.
extern "C" int proc_selfpid(void);
#include "LmiproUITrace.h"
struct LmiproTraceEvent {
  int code;
  uint64_t pc, a, b;
};
struct LmiproTraceState {
  uint64_t entries, returns, exceptions, interrupts;
  unsigned first_error_valid;
  LmiproTraceEvent entry, returned, exception, interrupt, signature, first_error;
};
static LmiproTraceState gLmiproTrace = {};
static volatile unsigned gLmiproTraceEnabled = 0;
static volatile int gLmiproTraceBusy = 0;
static volatile uint64_t gLmiproTraceDropped = 0;

extern "C" void lmipro_user_trace_event(unsigned kind, int code,
    uint64_t pc, uint64_t a, uint64_t b)
{
  if (!gLmiproTraceEnabled) return;
  int pid=proc_selfpid();
  if (pid!=1) {lmipro_ui_user_event(kind,code,pc,a,b,pid);return;}
  if (!__sync_bool_compare_and_swap(&gLmiproTraceBusy, 0, 1)) {
    __atomic_fetch_add(&gLmiproTraceDropped, 1, __ATOMIC_RELAXED);
    return;
  }
  LmiproTraceEvent event = {code, pc, a, b};
  if (kind == 1) {
    ++gLmiproTrace.entries; gLmiproTrace.entry = event;
  } else if (kind == 2) {
    ++gLmiproTrace.returns; gLmiproTrace.returned = event;
    // Carry is the real BSD user-return error flag; an error may be expected.
    if (code >= 0 && (b & (UINT64_C(1) << 29)) && !gLmiproTrace.first_error_valid) {
      gLmiproTrace.first_error = event; gLmiproTrace.first_error_valid = 1;
    }
  } else if (kind == 3) {
    ++gLmiproTrace.exceptions; gLmiproTrace.exception = event;
  } else if (kind == 4) {
    ++gLmiproTrace.interrupts; gLmiproTrace.interrupt = event;
  } else if (kind == 5) {
    gLmiproTrace.signature = event;
  }
  __sync_lock_release(&gLmiproTraceBusy);
}

static __attribute__((noinline)) void lmiproFormatUserTrace(char *out, size_t capacity)
{
  if (!out || !capacity) return;
  if (!__sync_bool_compare_and_swap(&gLmiproTraceBusy, 0, 1)) {
    snprintf(out, capacity, "PID1 trace busy; snapshot unavailable");
    return;
  }
  LmiproTraceState s = gLmiproTrace;
  __sync_lock_release(&gLmiproTraceBusy);
  snprintf(out, capacity,
    "PID1 svcIn=%llu svcOut=%llu userExc=%llu dropped=%llu\n"
    "in code=%d pc=%llx a0=%llx a1=%llx\n"
    "out code=%d pc=%llx r0=%llx cpsr=%llx\n"
    "exc ec=%x pc=%llx esr=%llx far=%llx\n"
    "EL0 irq=%llu pc=%llx sp=%llx cpsr=%llx\n"
    "fileSig stage=%d detail=%llx sigBytes=%llu base=%llx\n"
    "first BSD carry error valid=%u code=%d errno=%llu pc=%llx\n",
    (unsigned long long)s.entries, (unsigned long long)s.returns,
    (unsigned long long)s.exceptions, (unsigned long long)gLmiproTraceDropped,
    s.entry.code, (unsigned long long)s.entry.pc,
    (unsigned long long)s.entry.a, (unsigned long long)s.entry.b,
    s.returned.code, (unsigned long long)s.returned.pc,
    (unsigned long long)s.returned.a, (unsigned long long)s.returned.b,
    (unsigned)s.exception.code, (unsigned long long)s.exception.pc,
    (unsigned long long)s.exception.a, (unsigned long long)s.exception.b,
    (unsigned long long)s.interrupts, (unsigned long long)s.interrupt.pc,
    (unsigned long long)s.interrupt.a, (unsigned long long)s.interrupt.b,
    s.signature.code,(unsigned long long)s.signature.pc,
    (unsigned long long)s.signature.a,(unsigned long long)s.signature.b,
    s.first_error_valid, s.first_error.code,
    (unsigned long long)s.first_error.a, (unsigned long long)s.first_error.pc);
}
