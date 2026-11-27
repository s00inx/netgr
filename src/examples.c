#define _GNU_SOURCE

#include "ns.h"
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

// hardcoded trcaeroute for testing
int do_traceroute(const int fd, const char *toip) {
  return ns_exec(fd, (const char *[]){"traceroute", "-n", "-I", "-m", "10",
                                      "-w", "1", "-q", "1", toip, NULL});
}

int do_ping(const int fd, const char *toip) {
  return ns_exec(fd, (const char *[]){"ping", "-c", "3", toip, NULL});
}
