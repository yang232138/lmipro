// Opt-in diagnostic only. No I/O commands, locks, retries or error substitution.
extern "C" {
// Static kernel API from xnu-10063.141.1 kern/thread.h; IOKit's sanitized
// export header omits this MACH_KERNEL_PRIVATE declaration.
void thread_terminate_self(void);
extern volatile int lmipro_data_stage, lmipro_data_status, lmipro_data_readonly;
extern volatile uint32_t native_ram_block_ready;
extern volatile uint64_t native_ram_block_bytes, native_ram_block_reads,
    native_ram_block_writes, native_ram_block_failures;
}
static volatile unsigned gLmiproDataRootPhase = 0;
static unsigned gLmiproDataWatchSeconds = 0;

static void lmiproDataWatchThread(void *, wait_result_t)
{
  IOSleep(gLmiproDataWatchSeconds * 1000U);
  if (lmipro_data_stage != 4) {
    panic("LMIPRO DATA CHECKPOINT timeout=%u\n"
          "rootphase=%u pubState=%d completeWindows=%u\n"
          "dataStage=%d status=%d readonly=%d\n"
          "ramReady=%u bytes=%llu\nreads=%llu writes=%llu failures=%llu\n"
          "Diagnostic stop; not an inferred I/O error",
          gLmiproDataWatchSeconds, gLmiproDataRootPhase,
          gUfsPublishState, gPublishedWindows,
          lmipro_data_stage, lmipro_data_status, lmipro_data_readonly,
          native_ram_block_ready, (unsigned long long)native_ram_block_bytes,
          (unsigned long long)native_ram_block_reads,
          (unsigned long long)native_ram_block_writes,
          (unsigned long long)native_ram_block_failures);
  }
  thread_terminate_self();
}

static void lmiproStartDataWatch(void)
{
  unsigned seconds = 0;
  if (!PE_parse_boot_argn("lmipro-data-watch", &seconds, sizeof(seconds)) || !seconds)
    return;
  if (seconds < 5 || seconds > 120)
    panic("LMIPRO DATA CHECKPOINT invalid delay: %u", seconds);
  gLmiproDataWatchSeconds = seconds;
  thread_t thread = nullptr;
  kern_return_t error = kernel_thread_start(lmiproDataWatchThread, nullptr, &thread);
  if (error != KERN_SUCCESS)
    panic("LMIPRO DATA CHECKPOINT thread failed: %d", error);
  thread_deallocate(thread);
}
