#define _GNU_SOURCE

#include "ns.h"
#include <errno.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

int ns_exec(const int ns, const char *argv[]) {
  if (!argv || !argv[0])
    return -EINVAL;

  pid_t p = fork();
  if (p < 0)
    return -errno;

  if (p == 0) {
    if (setns(ns, CLONE_NEWNET) < 0) {
      perror("ns_exec: setns");
      _exit(126);
    }
    execvp(argv[0], (char *const *)argv);
    perror("ns_exec: execvp");
    _exit(127);
  }

  int st;
  if (waitpid(p, &st, 0) < 0)
    return -errno;

  if (!WIFEXITED(st))
    return -ECHILD;

  int code = WEXITSTATUS(st);
  if (code == 126)
    return -EACCES; // setns fail
  if (code == 127)
    return -ENOENT; // exec fail
  return code;
}

// hardcoded trcaeroute for testing
int do_traceroute(const int fd, const char *toip) {
  return ns_exec(fd, (const char *[]){"traceroute", "-n", "-I", "-m", "10",
                                      "-w", "1", "-q", "1", toip, NULL});
}

int do_ping(const int fd, const char *toip) {
  return ns_exec(fd, (const char *[]){"ping", "-c", "3", toip, NULL});
}
