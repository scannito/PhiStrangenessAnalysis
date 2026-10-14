#include <iostream>
#include <stdexcept>
// No global initializer anywhere. One function whose frame cannot be expressed as compact unwind
// (same shape as ORC's __orc_init_func: pre-indexed stp of x29/x30 without setting up x29).
__asm__(
  ".text\n"
  ".globl _dwarf_mode_fn\n"
  ".p2align 2\n"
  "_dwarf_mode_fn:\n"
  "  .cfi_startproc\n"
  "  stp x29, x30, [sp, #-16]!\n"
  "  .cfi_def_cfa_offset 16\n"
  "  .cfi_offset w30, -8\n"
  "  .cfi_offset w29, -16\n"
  "  ldp x29, x30, [sp], #16\n"
  "  .cfi_def_cfa_offset 0\n"
  "  .cfi_restore w30\n"
  "  .cfi_restore w29\n"
  "  ret\n"
  "  .cfi_endproc\n");
void thrower() { throw std::runtime_error("x"); }
void asmcase() { try { thrower(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << std::endl; } }
