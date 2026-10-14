# Cling on macOS arm64 (ROOT 6.36.10): exceptions thrown from interpreted code terminate, because the JIT is given no usable unwind info unless the module happens to contain ORC's `__orc_init_func`

## Summary

On macOS arm64 with ROOT 6.36.10, a `throw` that is caught by a `try`/`catch` in interpreted
code calls `std::terminate`, both in a plain macro and at the prompt. Adding to the macro a
global variable whose initializer runs code at load time (`inline int g = std::rand();`) makes
it work, although the variable is unrelated to the exception.

I traced this down to the object code the JIT emits and to how its unwind info is registered.
There are two independent steps, and the first one explains the trigger:

1. **No unwind info is emitted for the module's functions.** On arm64 Darwin LLVM emits an
   `__eh_frame` FDE only for functions whose compact-unwind encoding is `MODE_DWARF`; every
   other function gets only a `__compact_unwind` row, which the JIT does not use. A module of
   ordinary functions has no `MODE_DWARF` function, so it has **no `__eh_frame` section at all**.
2. **A module with a static initializer accidentally gets one.** ORC adds a function
   `__orc_init_func.<module>` to such a module. Its frame (`stp x29,x30,[sp,#-16]!` without
   setting x29) cannot be expressed as compact unwind, so it is `MODE_DWARF`, and the object
   then contains `__eh_frame` with an FDE for every function of the module. That is what makes
   `catch` work.

   Separately, a second hazard shows up even when `__eh_frame` is emitted: with a function of
   a different CFI size in the module, JITLink lays the section out with a 4-byte zero hole
   after the first CIE, and `walkLibunwindEHFrameSection` takes the zero length word for the
   end of the section and registers **no** FDE (details below).

This looks related to [root-project/root#7541](https://github.com/root-project/root/issues/7541)
and [llvm/llvm-project#49036](https://github.com/llvm/llvm-project/issues/49036), which report the
same symptom and are described as fixed with LLVM 20 / ROOT 6.38. I am reporting the mechanism
above, which I did not see spelled out there, and asking whether a fix or workaround is planned
for 6.36.x.

## Environment

- macOS 26.6.2 (25G83), arm64
- Apple clang 21.0.0 (clang-2100.1.1.101)
- ROOT v6-36-10-alice4-local1: the ROOT 6.36.10 of the ALICE software stack, built with
  aliBuild (`builtin_cling`, `libcxx`, `runtime_cxxmodules`). **This is ALICE's build, not a
  stock release.** I could not test a stock 6.36.10 or any ROOT >= 6.38.
- Cling and LLVM sources read for the analysis: the ALICE tree `v6-36-10-alice2`
  (`interpreter/cling`, `interpreter/llvm-project/llvm`); the binary tested is `alice4-local1`.

ROOT 6.36.04 (ALICE `alice9-local3`) is also installed but does not start on this SDK
(`std_darwin.modulemap:458: error: header "locale.h" ...`), so it is not compared.

## Reproducer

`fail.C`:

```cpp
#include <iostream>
#include <stdexcept>
void thrower() { throw std::runtime_error("x"); }
void fail() { try { thrower(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << std::endl; } }
```

```
$ root -l -b -q fail.C
libc++abi: terminating due to uncaught exception of type std::runtime_error: x
```

Expected `caught: x`. `work.C` is the same plus `inline int g = std::rand();` and prints it.

| Case (ROOT 6.36.10, `root -l -b -q`) | Result |
|---|---|
| `fail.C` | terminates |
| `-e 'try { throw 1; } catch (int) { printf("caught\n"); }'` | terminates |
| `fail.C+` (ACLiC) | catches |
| `fail.C` + a global with an initializer that runs code (`std::rand()`, `std::map`, `std::set`, `std::vector`, `std::shared_ptr`, a struct constructor, in any position in the file) | catches |
| the same, but the global is constant-folded (`constexpr int`, `std::string("a")+" b"`) | terminates |
| a `std::map` used as a local, or inside an uncalled function | terminates |
| `fail.C` with `CLING_JITLINK=0` (RuntimeDyld), with or without the initializer | terminates |

## Evidence

All the commands and scripts are in `tools/`; they use the local symbol names of this build and
break in `libCling` (`lldb` ran fine here even with Developer mode disabled).

### 1. What the JIT emits (objects dumped at `ObjectLinkingLayer::emit`)

`tools/dump_jit_objects.py` saves the object buffer handed to `ObjectLinkingLayer::emit` for
each module. For the macro's own module (`objects/*.o`; analysis in `traces/object_analysis.txt`):

| Module | sections | FDEs in `__eh_frame` | functions by compact-unwind mode |
|---|---|---|---|
| `fail.C` (`objects/fail_module.o`) | `__text __gcc_except_tab __cstring __compact_unwind` | **0, no `__eh_frame`** | 42 FRAME |
| `work.C`, with `inline int g = std::rand();` (`objects/work_module.o`) | the same + `__data` **+ `__eh_frame`** | **44** | 43 FRAME, **1 DWARF** |
| `asmcase.C`, no initializer, plus one function in `__asm__` with the same frame shape as ORC's init function (`objects/asm_module.o`) | + `__eh_frame` | **43** | 42 FRAME, **1 DWARF** |

The single `DWARF` function in `work_module.o` is `____orc_init_func.cling-module-10` (24 bytes). Its FDE
(`dwarfdump --eh-frame`, the last one in the section) is `DW_CFA_advance_loc 4; DW_CFA_def_cfa_offset 16;
DW_CFA_offset w30 -8; DW_CFA_offset w29 -16`: the CFA stays sp-based and x29 is never set up as a frame
pointer, which is the shape `generateCompactUnwindEncoding` cannot encode. The other 43 functions have the
usual `DW_CFA_def_cfa w29, 16` form.
In `asm_module.o` it is the hand-written `_dwarf_mode_fn`. In both, the module as a whole then
has an FDE for every function (44 of 44, 43 of 43). Compiling the same C++ ahead of time with
Apple clang gives `__compact_unwind` only and no `__eh_frame`, with or without the initializer.

### 2. What gets registered (`__register_frame` / `__cxa_throw` interposed)

`interpose.cpp` (`DYLD_INSERT_LIBRARIES`, run `$ROOTSYS/bin/root.exe`, not the `root` launcher)
logs every FDE passed to the unwinder with the code range it covers (decoded from its CIE) and
whether the PC of a `throw` is inside one.

| | `fail.C` | `work.C` |
|---|---|---|
| FDEs registered before the throw | 1, and unrelated (44 bytes, ~4.6 MB away; probably the stub of `___cxa_atexit`, the only other emitted object with an FDE) | 45 |
| address of `thrower` | `0x1007f8000` (size 0x84), **not covered** | `0x100bd80a4`, covered by an FDE of exactly `[0x100bd80a4, 0x100bd8128)` |
| `catch` | terminates | works |

(Logs in `logs/`; addresses vary per run.)

### 3. The registration hazard, shown with `asmcase.C`

`asm_module.o` does contain 43 FDEs, yet `asmcase.C` still terminates and only the 1 unrelated FDE
is registered. `tools/trace_registration.py` shows `InProcessEHFrameRegistrar::registerEHFrames`
being called for the module, and `tools/dump_ehframe_range.py` dumps the range it receives
(`traces/ehframe_asm_module.bin`, 1776 bytes). First bytes:

```
00000000: 1000 0000 0000 0000 017a 5200 0178 1e01  ........zR..x..     <- CIE, 20 bytes
00000010: 100c 1f00 0000 0000 2400 0000 1c00 0000                      <- +0x14: 4 zero bytes, then an FDE at +0x18
```

Walking it the way `walkLibunwindEHFrameSection` does gives 1 CIE and 0 FDE; walking it tolerantly
(skipping the zero word) gives 2 CIE and 42 FDE. In `work.C` (`traces/ehframe_work_module.bin`) the
first FDE follows the CIE directly, at `+0x14`, with no hole, and the walk finds 2 CIE and 44 FDE.
The record sizes in memory are `[20, 36, 40, 40, ...]` for `work.C` and `[20, pad 4, 40, 40, ...]`
for `asmcase.C`. My reading is that JITLink aligns the first FDE block to 8, which leaves a 4-byte
hole after the 20-byte CIE only when that FDE's size is a multiple of 8. I tried to confirm this by
shortening the CFI of the asm function, but its FDE stayed at 40 bytes, so this part of the
explanation is consistent with the data but **not confirmed**.

### 4. RuntimeDyld (`CLING_JITLINK=0`)

The same registration counts (1 without the initializer, 45 with it), but in both cases the
registered ranges do not cover the functions, so it fails either way. This is similar to what
llvm#49036 discusses about FDE addresses not matching the code.

## Source references

- `llvm/lib/Target/AArch64/MCTargetDesc/AArch64AsmBackend.cpp:605-611`:
  `generateCompactUnwindEncoding` returns `MODE_DWARF` for a non-canonical personality or any CFI it
  cannot encode (the init function's `def_cfa_offset` + `offset x30/x29` pair is one).
- `llvm/lib/MC/MCObjectFileInfo.cpp:72-87`: `SupportsCompactUnwindWithoutEHFrame = true` and
  `OmitDwarfIfHaveCompactUnwind` on arm64 Darwin.
- `llvm/lib/MC/MCDwarf.cpp:1863, 1885`: `NeedsEHFrameSection` and the early return that skips
  `__eh_frame` when no function needs DWARF. I have **not** reconciled the observed "FDE for every
  function once one is `MODE_DWARF`" with the per-frame skip a few lines further down in the same
  function, which would suggest only the `MODE_DWARF` function gets one.
- `llvm/lib/ExecutionEngine/Orc/TargetProcess/RegisterEHFrames.cpp:91-120`
  (`walkLibunwindEHFrameSection`, loop condition `Size != 0` at line 97): stops at the first
  zero-length word and assumes records are contiguous.
- `llvm/lib/ExecutionEngine/Orc/LLJIT.cpp:~134`: the init-function prefix `__orc_init_func`.
- `cling/lib/Interpreter/IncrementalExecutor.cpp:156-176` and `IncrementalJIT.cpp:507-508`:
  `runStaticInitializersOnce` and the `EHFrameRegistrationPlugin`.

## Questions

1. Is this the same root cause as #7541 / llvm#49036? The dependence on a static initializer, and the
   fact that it works because of ORC's init function and not because of the initializer itself, is
   the new piece.
2. Is the stop-at-zero loop in `walkLibunwindEHFrameSection` against JITLink's `__eh_frame` layout
   a known problem? It looks like it would matter on its own once `__eh_frame` is emitted.
3. The fix is described as LLVM 20 / ROOT 6.38 (listed as fixed in 6.42). Is a backport or
   workaround planned for 6.36.x? The ALICE software stack is on 6.36.
4. Can anyone confirm the behaviour on stock ROOT 6.36.10 and on >= 6.38 on macOS 26?

## Workarounds for users

- Compile the code (ACLiC `.C+`, or a standalone binary): exceptions are caught.
- A global with a load-time initializer in the same interpreted unit makes `catch` work, but it
  depends on ORC's init function and on the section layout, and is not something to rely on.

## What I did not establish

- Stock ROOT (only the ALICE build was available), any ROOT >= 6.38, other macOS versions.
- Why, in the `MCDwarf.cpp` emission, the whole module gets FDEs once one function is `MODE_DWARF`
  (see above).
- The exact condition that produces the 4-byte hole after the CIE (sketched above, not confirmed).

## Attachments

`fail.C`, `work.C`, `asmcase.C`, `failaddr.C`, `workaddr.C`, `interpose.cpp`, `tools/`, `objects/`,
`traces/`, `logs/`.
