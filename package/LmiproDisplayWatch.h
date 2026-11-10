// Read-only console/registry evidence for selecting the LMIPRO display backend.
// Included after the public IOKit/pexpert headers in LmiproUfsBlock.cpp.
// No mapping, framebuffer stores, service creation or display/power operations.
#if defined(LMIPRO_DISPLAY_BOARD) && LMIPRO_DISPLAY_BOARD
extern "C" unsigned lmipro_display_report(char *,unsigned);
extern "C" unsigned lmipro_ui_trace_report(char *,unsigned);
#endif
static unsigned lmiproConsoleLayout(const PE_Video &v, uint64_t *bytes)
{
  *bytes = 0;
  if (!v.v_baseAddr || !v.v_width || !v.v_height) return 1;
  if (v.v_width > 16384 || v.v_height > 16384 || v.v_depth != 32) return 2;
  if (v.v_rowBytes < v.v_width * 4 || v.v_rowBytes > UINT64_MAX / v.v_height) return 3;
  uint64_t span = v.v_rowBytes * v.v_height;
  if (v.v_offset > UINT64_MAX - v.v_baseAddr ||
      span > UINT64_MAX - v.v_baseAddr - v.v_offset) return 4;
  *bytes = span;
  return 0; // Arithmetic only; does not qualify mapping, format or scanout.
}

// Query the nonempty matched IOService population, then filter in this iterator.
// copyExistingServices returns NULL both for an empty result and allocation failure;
// asking it for the target class directly cannot distinguish those cases.
// -1: query unavailable, -2: registry mutation, -3: scan limit, 33: count cap.
// Both caller-owned references are released; returned objects stay iterator owned.
static __attribute__((noinline)) int lmiproCountDisplayServices(const char *name)
{
  OSDictionary *match = IOService::serviceMatching("IOService");
  if (!match) return -1;
  OSIterator *it = IOService::getMatchingServices(match);
  match->release();
  if (!it) return -1;
  int count = 0;
  unsigned seen = 0;
  while (seen < 4096) {
    OSObject *object = it->getNextObject();
    if (!object) break;
    ++seen;
    if (object->metaCast(name) && ++count == 33) break;
  }
  if (seen == 4096 && it->getNextObject()) count = -3;
  if (!it->isValid()) count = -2;
  it->release();
  return count;
}

static __attribute__((noinline)) void lmiproFormatDisplaySnapshot(char *out, size_t capacity)
{
  PE_Video video = {};
  int status = PE_current_console(&video);
  // PE_Video's fixed array is not guaranteed to contain a terminator.
  video.v_pixelFormat[sizeof(video.v_pixelFormat) - 1] = 0;
  uint64_t bytes = 0;
  unsigned layout = lmiproConsoleLayout(video, &bytes);
  int mobile = lmiproCountDisplayServices("IOMobileFramebuffer");
  int legacy = lmiproCountDisplayServices("IOMobileFramebufferLegacy");
  int surface = lmiproCountDisplayServices("IOSurfaceRoot");
  int gop = lmiproCountDisplayServices("NativeGOPPresenter");
  snprintf(out, capacity,
    "GOP status=%d layout=%u base=%llx\n"
    "mode=%llux%llu depth=%llu stride=%llu\n"
    "offset=%llx length=%llx scanBytes=%llx\n"
    "rotate=%u scale=%u display=%llu format=%s\n"
    "published MobileFB=%d LegacyFB=%d IOSurface=%d GOP=%d\n"
    "counts: -1 query unavailable; -2 invalid; -3 scan limit; 33 capped\n",
    status, layout, (unsigned long long)video.v_baseAddr,
    (unsigned long long)video.v_width, (unsigned long long)video.v_height,
    (unsigned long long)video.v_depth, (unsigned long long)video.v_rowBytes,
    (unsigned long long)video.v_offset, (unsigned long long)video.v_length,
    (unsigned long long)bytes, (unsigned)video.v_rotate, (unsigned)video.v_scale,
    (unsigned long long)video.v_display, video.v_pixelFormat,
    mobile, legacy, surface, gop);
}

static __attribute__((noinline)) void lmiproDisplayCheckpoint(unsigned seconds, const char *processes)
{
  unsigned enabled = 0;
  if (!PE_parse_boot_argn("lmipro-display-watch", &enabled, sizeof(enabled)) || !enabled) return;
  char display[576];
  lmiproFormatDisplaySnapshot(display, sizeof(display));
  char detail[1024] = {};
  char ui[1200] = {};
#if defined(LMIPRO_DISPLAY_BOARD) && LMIPRO_DISPLAY_BOARD
  lmipro_display_report(detail,sizeof(detail));
  lmipro_ui_trace_report(ui,sizeof(ui));
#endif
  panic("LMIPRO DISPLAY CHECKPOINT timeout=%u\n%s%s%s"
    "process sample pid<=64: %s\n"
    "Diagnostic stop; display not initialized by this observer",
    seconds, display, detail, ui, processes);
}
