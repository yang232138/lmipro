// Opt-in diagnostic only. No storage commands, retries or error substitution.
extern "C" {
extern volatile int lmipro_data_stage, lmipro_data_status, lmipro_data_readonly;
extern volatile int lmipro_init_stage, lmipro_init_error;
extern char lmipro_init_path[128];
// Exact public debugging API in pinned xnu-10063.141.1 bsd/sys/proc.h.
void proc_name(int pid, char *buffer, int size);
extern volatile uint32_t native_ram_block_ready;
extern volatile uint64_t native_ram_block_bytes, native_ram_block_reads,
    native_ram_block_writes, native_ram_block_failures;
}
static volatile unsigned gLmiproDataRootPhase = 0;
static unsigned gLmiproDataWatchSeconds = 0;
#include "LmiproUserTrace.h"

// Bounded read-only sample, not a complete process census. proc_name owns and
// drops its proc_find reference; no raw proc pointers or transition waits here.
static __attribute__((noinline)) void lmiproSampleProcesses(char *buffer, size_t capacity)
{
  if (!buffer || !capacity) return;
  buffer[0] = 0;
  size_t used = 0;
  for (int pid = 1; pid <= 64 && used + 1 < capacity; ++pid) {
    char name[17] = {};
    proc_name(pid, name, sizeof(name));
    name[sizeof(name) - 1] = 0;
    if (!name[0]) continue;
    int count = snprintf(buffer + used, capacity - used, "%d:%s ", pid, name);
    if (count < 0 || (size_t)count >= capacity - used) break;
    used += (size_t)count;
  }
}

static void lmiproDataWatchThread(void *, wait_result_t)
{
  IOSleep(gLmiproDataWatchSeconds * 1000U);
  char processes[384];
  lmiproSampleProcesses(processes, sizeof(processes));
  char trace[640];
  lmiproFormatUserTrace(trace, sizeof(trace));
  // The opt-in snapshot must also report stage 4. Silence at success cannot
  // distinguish a later userspace stall from a watch that never started.
  panic("LMIPRO DATA CHECKPOINT timeout=%u\n"
          "rootphase=%u pubState=%d completeWindows=%u\n"
          "dataStage=%d status=%d readonly=%d\n"
          "ramReady=%u bytes=%llu\nreads=%llu writes=%llu failures=%llu\n"
          "initStage=%d execError=%d path=%s\n"
          "process sample pid<=64: %s\n"
          "%s"
          "Diagnostic stop; not an inferred I/O error",
          gLmiproDataWatchSeconds, gLmiproDataRootPhase,
          gUfsPublishState, gPublishedWindows,
          lmipro_data_stage, lmipro_data_status, lmipro_data_readonly,
          native_ram_block_ready, (unsigned long long)native_ram_block_bytes,
          (unsigned long long)native_ram_block_reads,
          (unsigned long long)native_ram_block_writes,
          (unsigned long long)native_ram_block_failures,
          lmipro_init_stage, lmipro_init_error, lmipro_init_path,
          processes, trace);
}

static void lmiproStartDataWatch(void)
{
  unsigned seconds = 0;
  if (!PE_parse_boot_argn("lmipro-data-watch", &seconds, sizeof(seconds)) || !seconds)
    return;
  if (seconds < 5 || seconds > 120)
    panic("LMIPRO DATA CHECKPOINT invalid delay: %u", seconds);
  gLmiproDataWatchSeconds = seconds;
  gLmiproTraceEnabled = 1;
  thread_t thread = nullptr;
  kern_return_t error = kernel_thread_start(lmiproDataWatchThread, nullptr, &thread);
  if (error != KERN_SUCCESS)
    panic("LMIPRO DATA CHECKPOINT thread failed: %d", error);
  thread_deallocate(thread);
}
