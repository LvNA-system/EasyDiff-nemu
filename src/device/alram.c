#include "common.h"

#include <sys/time.h>
#include <signal.h>

#define TIMER_HZ 100
#define MAX_HANDLER 8

typedef void (*alarm_handler_t) (void);
static alarm_handler_t handler[MAX_HANDLER] = {};
static int idx = 0;
static struct itimerval it = {};

void add_alarm_handle(void *h) {
  assert(idx < MAX_HANDLER);
  handler[idx ++] = h;
}

static void alarm_sig_handler(int signum) {
  int i;
  for (i = 0; i < idx; i ++) {
    handler[i]();
  }

  int ret = setitimer(ITIMER_VIRTUAL, &it, NULL);
  Assert(ret == 0, "Can not set timer");
}

void init_alarm(void) {
  struct sigaction s;
  memset(&s, 0, sizeof(s));
  s.sa_handler = alarm_sig_handler;
  int ret = sigaction(SIGVTALRM, &s, NULL);
  Assert(ret == 0, "Can not set signal handler");

  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 1000000 / TIMER_HZ;
  ret = setitimer(ITIMER_VIRTUAL, &it, NULL);
  Assert(ret == 0, "Can not set timer");
}
