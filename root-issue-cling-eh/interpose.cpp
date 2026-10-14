// Logs how the JIT registers unwind info, and whether the PC of a throw is covered.
// Build: clang++ -dynamiclib -std=c++17 interpose.cpp -o interpose.dylib
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <typeinfo>
#include <vector>
#include <unwind.h>
#include <cstdlib>
#include <mach-o/dyld.h>
#include <string>

struct Range { uintptr_t lo, hi; const char* how; char detail[200]; };
static std::vector<Range> g_ranges;
static int g_nreg = 0;

#define DYLD_INTERPOSE(_replacement, _replacee) \
  __attribute__((used)) static struct { const void* replacement; const void* replacee; } _interpose_##_replacee \
  __attribute__((section("__DATA,__interpose"))) = {(const void*)(uintptr_t)&_replacement, (const void*)(uintptr_t)&_replacee};

extern "C" {
void __register_frame(const void*);
void __deregister_frame(const void*);
int __unw_add_dynamic_fde(uintptr_t);
int __unw_add_dynamic_eh_frame_section(uintptr_t);
void __cxa_throw(void*, std::type_info*, void (*)(void*));
}

// Decode one FDE. The pointer encoding is read from the CIE it points to, not assumed.
static uint64_t uleb(const uint8_t*& p) { uint64_t r = 0; int sh = 0; while (true) { uint8_t b = *p++; r |= (uint64_t)(b & 0x7f) << sh; if (!(b & 0x80)) break; sh += 7; } return r; }
static int64_t sleb(const uint8_t*& p) { int64_t r = 0; int sh = 0; uint8_t b; do { b = *p++; r |= (int64_t)(b & 0x7f) << sh; sh += 7; } while (b & 0x80); if (sh < 64 && (b & 0x40)) r |= -((int64_t)1 << sh); return r; }
static int64_t readEnc(const uint8_t*& p, uint8_t enc, bool& okFmt) {
  okFmt = true; int64_t v = 0;
  switch (enc & 0x0f) {
    case 0x00: memcpy(&v, p, 8); p += 8; break;               // absptr
    case 0x03: { uint32_t x; memcpy(&x, p, 4); v = x; p += 4; break; }
    case 0x0b: { int32_t x; memcpy(&x, p, 4); v = x; p += 4; break; }
    case 0x0c: memcpy(&v, p, 8); p += 8; break;
    default: okFmt = false;
  }
  return v;
}
static bool decodeFDE(const uint8_t* rec, Range& out) {
  uint32_t len; memcpy(&len, rec, 4);
  if (len == 0 || len == 0xffffffffu) return false;
  uint32_t cieptr; memcpy(&cieptr, rec + 4, 4);
  if (cieptr == 0) return false;                    // a CIE, not an FDE
  const uint8_t* cie = rec + 4 - cieptr;
  const uint8_t* c = cie + 8;                       // skip length + id
  uint8_t version = *c++;
  const char* aug = (const char*)c; c += strlen(aug) + 1;
  (void)version; uleb(c); sleb(c); uleb(c);        // code align, data align, RA register
  uint8_t enc = 0;
  if (aug[0] == 'z') { uleb(c); for (const char* a = aug + 1; *a; ++a) { if (*a == 'R') enc = *c++; else if (*a == 'L') c++; else if (*a == 'P') { uint8_t pe = *c++; bool ok; readEnc(c, pe, ok); } } }
  const uint8_t* p = rec + 8;
  bool ok1, ok2;
  const uint8_t* at = p;
  int64_t pcb = readEnc(p, enc, ok1);
  int64_t rng = readEnc(p, enc & 0x0f, ok2);
  if (!ok1 || !ok2) return false;
  uintptr_t lo = (enc & 0x70) == 0x10 ? (uintptr_t)at + pcb : (uintptr_t)pcb;
  out = {lo, lo + (uintptr_t)rng, "", ""};
  // details: CIE augmentation, FDE augmentation length, and the CFI program of the FDE
  uint64_t fdeAug = 0;
  if (aug[0] == 'z') fdeAug = uleb(p);
  const uint8_t* lsda = p; p += fdeAug;
  const uint8_t* end = rec + 4 + len;
  std::string hex;
  for (const uint8_t* q = p; q < end && hex.size() < 60; ++q) { char b[4]; snprintf(b, sizeof b, "%02x", *q); hex += b; }
  snprintf(out.detail, sizeof out.detail, "CIE aug=\"%s\" fde_aug_len=%llu cfi=%s", aug, (unsigned long long)fdeAug, hex.c_str());
  (void)lsda;
  return true;
}

static void addFDE(const uint8_t* rec, const char* how) {
  Range r;
  if (decodeFDE(rec, r)) { r.how = how; g_ranges.push_back(r); }
}

static void my_register_frame(const void* p) {
  g_nreg++;
  Range r;
  bool ok = decodeFDE((const uint8_t*)p, r);
  if (ok) { r.how = "__register_frame"; g_ranges.push_back(r); }
  fprintf(stderr, "[eh] __register_frame(%p)  %s\n", p, ok ? "FDE" : "(CIE/other)");
  if (ok) fprintf(stderr, "[eh]    covers [%#lx, %#lx)  size=%lu  %s\n", (unsigned long)r.lo, (unsigned long)r.hi, (unsigned long)(r.hi-r.lo), r.detail);
  __register_frame(p);
}
static int my_add_fde(uintptr_t fde) {
  g_nreg++;
  addFDE((const uint8_t*)fde, "__unw_add_dynamic_fde");
  fprintf(stderr, "[eh] __unw_add_dynamic_fde(%#lx)\n", (unsigned long)fde);
  return __unw_add_dynamic_fde(fde);
}
static int my_add_section(uintptr_t sect) {
  g_nreg++;
  // walk the records of the section until the zero terminator
  int n = 0; const uint8_t* p = (const uint8_t*)sect;
  while (true) {
    uint32_t len; memcpy(&len, p, 4);
    if (len == 0) break;
    Range r; if (decodeFDE(p, r)) { r.how = "__unw_add_dynamic_eh_frame_section"; g_ranges.push_back(r); n++; }
    p += 4 + len;
  }
  fprintf(stderr, "[eh] __unw_add_dynamic_eh_frame_section(%#lx)  %d FDE(s)\n", (unsigned long)sect, n);
  return __unw_add_dynamic_eh_frame_section(sect);
}
static void my_deregister(const void* p) { __deregister_frame(p); }


// ---- optional: dump the object files the JIT handed to the GDB JIT interface -------------
// Needs CLING_JITLINK=0 CLING_DEBUG=1 (RuntimeDyld registers its objects with the GDB listener).
// EH_DUMP_DIR=<dir> and EH_JITDESC_OFFSET=<link-time address of ___jit_debug_descriptor in libCling,
// from `nm -a libCling.*.so | grep jit_debug_descriptor`>.
struct jit_code_entry { jit_code_entry* next_entry; jit_code_entry* prev_entry; const char* symfile_addr; uint64_t symfile_size; };
struct jit_descriptor { uint32_t version; uint32_t action_flag; jit_code_entry* relevant_entry; jit_code_entry* first_entry; };

static void dumpJitObjects() {
  const char* dir = getenv("EH_DUMP_DIR");
  const char* off = getenv("EH_JITDESC_OFFSET");
  if (!dir || !off) return;
  uintptr_t offset = strtoull(off, nullptr, 16);
  for (uint32_t i = 0; i < _dyld_image_count(); ++i) {
    const char* name = _dyld_get_image_name(i);
    if (!strstr(name, "libCling.")) continue;
    jit_descriptor* d = (jit_descriptor*)(offset + _dyld_get_image_vmaddr_slide(i));
    fprintf(stderr, "[eh] jit descriptor in %s: version=%u first_entry=%p\n", name, d->version, (void*)d->first_entry);
    int n = 0;
    for (jit_code_entry* e = d->first_entry; e; e = e->next_entry, ++n) {
      std::string path = std::string(dir) + "/obj_" + std::to_string(n) + ".o";
      FILE* f = fopen(path.c_str(), "wb");
      if (f) { fwrite(e->symfile_addr, 1, e->symfile_size, f); fclose(f); }
      fprintf(stderr, "[eh]   object %d: %llu bytes -> %s\n", n, (unsigned long long)e->symfile_size, path.c_str());
    }
    return;
  }
}

static void my_cxa_throw(void* obj, std::type_info* ti, void (*d)(void*)) {
  uintptr_t pc = (uintptr_t)__builtin_return_address(0);
  const Range* hit = nullptr;
  for (const auto& r : g_ranges) if (pc >= r.lo && pc < r.hi) hit = &r;
  fprintf(stderr, "[eh] __cxa_throw of %s from pc=%#lx : %zu range(s) registered so far (%d calls), pc %s\n",
          ti->name(), (unsigned long)pc, g_ranges.size(), g_nreg,
          hit ? "IS covered by a registered FDE" : "is NOT covered by any registered FDE");
  dumpJitObjects();
  __cxa_throw(obj, ti, d);
}

DYLD_INTERPOSE(my_register_frame, __register_frame)
DYLD_INTERPOSE(my_deregister, __deregister_frame)
DYLD_INTERPOSE(my_add_fde, __unw_add_dynamic_fde)
DYLD_INTERPOSE(my_add_section, __unw_add_dynamic_eh_frame_section)
DYLD_INTERPOSE(my_cxa_throw, __cxa_throw)
