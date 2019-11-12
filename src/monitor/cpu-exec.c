#include "nemu.h"
#include "monitor/monitor.h"
#include "monitor/watchpoint.h"
#include "monitor/diff-test.h"
#include <stdlib.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INSTR_TO_PRINT 10

// control when the log is printed
#define LOG_START (0)
// restrict the size of log file
#define LOG_END   (1024 * 1024 * 50)

NEMUState nemu_state = {.state = NEMU_STOP};
static uint64_t g_nr_guest_instr = 0;

vaddr_t exec_once(void);
void asm_print(vaddr_t ori_pc, int instr_len, bool print_flag);

int goodtrap(void) {
  return (nemu_state.state == NEMU_END && nemu_state.halt_ret == 0);
}

void interpret_rtl_exit(int state, vaddr_t halt_pc, uint32_t halt_ret) {
  nemu_state = (NEMUState) { .state = state, .halt_pc = halt_pc, .halt_ret = halt_ret };
}

void monitor_statistic(void) {
  Log("total guest instructions = %ld", g_nr_guest_instr);
}

bool log_enable(void) {
  return (g_nr_guest_instr >= LOG_START) && (g_nr_guest_instr <= LOG_END);
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  switch (nemu_state.state) {
    case NEMU_END: case NEMU_ABORT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  for (; n > 0; n --) {
    __attribute__((unused)) vaddr_t ori_pc = cpu.pc;

    /* Execute one instruction, including instruction fetch,
     * instruction decode, and the actual execution. */
    __attribute__((unused)) vaddr_t seq_pc = exec_once();

    difftest_step(ori_pc, cpu.pc);

    g_nr_guest_instr ++;

#ifdef DEBUG
    asm_print(ori_pc, seq_pc - ori_pc, n < MAX_INSTR_TO_PRINT);

    /* TODO: check watchpoints here. */
    WP *wp = scan_watchpoint();
    if(wp != NULL) {
      printf("\n\nHint watchpoint %d at address " FMT_WORD ", expr = %s\n", wp->NO, ori_pc, wp->expr);
      printf("old value = " FMT_WORD "\nnew value = " FMT_WORD "\n", wp->old_val, wp->new_val);
      wp->old_val = wp->new_val;
      return;
    }
#endif

#ifdef HAS_IOE
    extern void device_update();
    device_update();
#endif

    if (nemu_state.state != NEMU_RUNNING) break;
  }

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      Log("nemu: %s\33[0m at pc = " FMT_WORD "\n\n",
          (nemu_state.state == NEMU_ABORT ? "\33[1;31mABORT" :
           (nemu_state.halt_ret == 0 ? "\33[1;32mHIT GOOD TRAP" : "\33[1;31mHIT BAD TRAP")),
          nemu_state.halt_pc);
      monitor_statistic();
      if (nemu_state.state == NEMU_ABORT) abort();
  }
}
