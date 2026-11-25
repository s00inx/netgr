#define _GNU_SOURCE

#include "ns.h"
#include <errno.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

// hardcoded ping for checking availability
// will be removed soon
int do_ping(const struct node *from, const char *toip) {
  pid_t p = fork();
  if (p < 0) {
    return -errno;
  }

  if (p == 0) {
    if (setns(from->fd, CLONE_NEWNET) < 0)
      _exit(127);

    execlp("ping", "ping", "-c", "3", toip, NULL);
    _exit(127);
  }

  int st;
  waitpid(p, &st, 0);
  if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
    return 0;
  return -1;
}

// hardcoded trcaeroute for testing
int do_traceroute(const struct node *from, const char *toip) {
  pid_t p = fork();
  if (p < 0)
    return -errno;

  if (p == 0) {
    if (setns(from->fd, CLONE_NEWNET) < 0)
      _exit(127);

    execlp("traceroute", "traceroute",
           "-n",       // без DNS (в netns нет резолвера!)
           "-I",       // ICMP вместо UDP (надёжнее через роутер)
           "-m", "10", // max 10 hops
           "-w", "1",  // таймаут 1 сек на hop
           "-q", "1",  // 1 probe на hop (быстрее)
           toip, NULL);
    _exit(127);
  }

  int st;
  waitpid(p, &st, 0);
  if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
    return 0;
  return -1;
}
