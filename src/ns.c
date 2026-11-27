#define _GNU_SOURCE

#include "ns.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <libmnl/libmnl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sched.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// set ip forward from dest ns
int ns_ip_fwd(void) {
  int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
  if (fd < 0) {
    return -errno;
  }

  if (write(fd, "1", 1) != 1) {
    if (close(fd) < 0) {
      return -errno;
    };

    return -errno;
  }

  close(fd);
  return 0;
}

// add default gateway
int ns_add_default_route(struct mnl_socket *nl, const char *oif_name,
                         const char *gw_ip) {
  char buf[MNL_SKBUFSZ];
  memset(buf, 0, sizeof(buf));

  struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
  nlh->nlmsg_type = RTM_NEWROUTE;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_REPLACE;
  nlh->nlmsg_seq = next_seq();

  struct rtmsg *rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(*rtm));
  rtm->rtm_family = AF_INET;
  rtm->rtm_dst_len = 0;
  rtm->rtm_table = RT_TABLE_MAIN;
  rtm->rtm_protocol = RTPROT_BOOT;
  rtm->rtm_scope = RT_SCOPE_UNIVERSE;
  rtm->rtm_type = RTN_UNICAST;

  int oif = (int)if_nametoindex(oif_name);
  if (oif == 0)
    return -ENODEV;

  struct in_addr gw;
  if (inet_pton(AF_INET, gw_ip, &gw) != 1)
    return -EINVAL;

  mnl_attr_put_u32(nlh, RTA_OIF, (uint32_t)oif);
  mnl_attr_put(nlh, RTA_GATEWAY, sizeof(gw), &gw);

  return mnl_ack(nl, nlh, buf, sizeof(buf));
}

// execute in ns
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

// enter to namespace via fd
int ns_enter(int to) {
  if (to < 0)
    return -EBADF;

  int from = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
  if (from < 0)
    return -errno;

  if (setns(to, CLONE_NEWNET) < 0) {
    int e = errno;
    close(from);
    return -e;
  }
  return from;
}

// exit to from namespace via fd
int ns_exit(int to) {
  if (to < 0)
    return -EBADF;

  if (setns(to, CLONE_NEWNET) < 0) {
    int e = errno;
    close(to);
    return -e;
  }
  close(to);
  return 0;
}
