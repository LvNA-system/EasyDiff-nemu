#include "rtl/rtl.h"
#include "csr.h"
#include <setjmp.h>

#define INTR_BIT (1ULL << 63)
enum {
  IRQ_USIP, IRQ_SSIP, IRQ_HSIP, IRQ_MSIP,
  IRQ_UTIP, IRQ_STIP, IRQ_HTIP, IRQ_MTIP,
  IRQ_UEIP, IRQ_SEIP, IRQ_HEIP, IRQ_MEIP
};

void raise_intr(word_t NO, vaddr_t epc) {
  // TODO: Trigger an interrupt/exception with ``NO''

  word_t deleg = (NO & INTR_BIT ? mideleg->val : medeleg->val);
  bool delegS = ((deleg & (1 << (NO & 0xf))) != 0) && (cpu.mode < MODE_M);

  if (delegS) {
    scause->val = NO;
    sepc->val = epc;
    mstatus->spp = cpu.mode;
    mstatus->spie = mstatus->sie;
    mstatus->sie = 0;
    cpu.mode = MODE_S;
    rtl_li(&s0, stvec->val);
  } else {
    mcause->val = NO;
    mepc->val = epc;
    mstatus->mpp = cpu.mode;
    mstatus->mpie = mstatus->mie;
    mstatus->mie = 0;
    cpu.mode = MODE_M;
    rtl_li(&s0, mtvec->val);
  }

  rtl_jr(&s0);
}

bool isa_query_intr(void) {
  word_t intr_vec = mie->val & mip->val;
  const int priority [] = {
    IRQ_MEIP, IRQ_MSIP, IRQ_MTIP,
    IRQ_SEIP, IRQ_SSIP, IRQ_STIP,
    IRQ_UEIP, IRQ_USIP, IRQ_UTIP
  };
  int i;
  for (i = 0; i < 9; i ++) {
    int irq = priority[i];
    if (intr_vec & (1 << irq)) {
      bool deleg = (mideleg->val & (1 << irq)) != 0;
      bool global_enable = (deleg ? ((cpu.mode == MODE_S) && mstatus->sie) || (cpu.mode < MODE_S) :
          ((cpu.mode == MODE_M) && mstatus->mie) || (cpu.mode < MODE_M));
      if (global_enable) {
        raise_intr(irq | INTR_BIT, cpu.pc);
        return true;
      }
    }
  }
  return false;
}

jmp_buf intr_buf;
void longjmp_raise_intr(uint32_t NO) {
  longjmp(intr_buf, NO + 1);
}
