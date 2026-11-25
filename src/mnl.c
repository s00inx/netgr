// low level functions that are working with netlink via libmnl
#define _GNU_SOURCE

// default headers
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/veth.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
// libmnl headers
#include <libmnl/libmnl.h>
// my headers
#include "ns.h"

#define MNL_SKBUFSZ                                                            \
  2048 // MNL_SKBUFSZ is macro so i can't use vla for stack arrays

static uint32_t seq = 0;
// next sequence number for netlink messages
static uint32_t next_seq() { return ++seq; }

// init socket for managing netlink
struct mnl_socket *initmnl() {
  struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);
  if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
    mnl_socket_close(nl);
    return NULL;
  };

  return nl; // check for NULL
}

// helper for adding a nested attrs to nlmsg
static inline struct ifinfomsg *enterifi(struct nlmsghdr *nlh) {
  struct ifinfomsg *ifi = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifi));
  ifi->ifi_family = AF_UNSPEC;
  ifi->ifi_change = 0xFFFFFFFF;

  return ifi;
}

// helper to send netlink message and check for ack
static int mnl_ack(struct mnl_socket *nl, struct nlmsghdr *nlh, char *buf,
                   const size_t buflen) {
  if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
    return -errno;
  }

  ssize_t ret = mnl_socket_recvfrom(nl, buf, buflen);
  if (ret < 0)
    return -errno;

  return mnl_cb_run(buf, (size_t)ret, nlh->nlmsg_seq, mnl_socket_get_portid(nl),
                    NULL, NULL);
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

// set up new netns fd
int node_apply(struct node *n) {
  int cur = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
  if (cur < 0) {
    return -errno;
  }

  if (unshare(CLONE_NEWNET) < 0) {
    close(cur);
    return -errno;
  }

  int fd = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    setns(cur, CLONE_NEWNET);
    close(cur);
    return -errno;
  }

  if (setns(cur, CLONE_NEWNET) < 0) {
    close(fd);
    close(cur);
    return -errno;
  };

  n->fd = fd;
  close(cur);
  return 0;
}

// create a pair of veth-ifaces named "in_name" and "out_name"
int veth_pair(struct mnl_socket *nl, const char *in_name,
              const char *out_name) {
  if (strlen(in_name) >= IFNAMSIZ || strlen(out_name) >= IFNAMSIZ) {
    return -EINVAL;
  }

  char buf[MNL_SKBUFSZ];
  memset(buf, 0, sizeof(buf));

  struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
  nlh->nlmsg_type = RTM_NEWLINK;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
  nlh->nlmsg_seq = next_seq();

  enterifi(nlh);
  mnl_attr_put_strz(nlh, IFLA_IFNAME, in_name);

  // first nest linkinfo
  struct nlattr *li = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
  mnl_attr_put_strz(nlh, IFLA_INFO_KIND, "veth");

  // iface info data 2 nest
  struct nlattr *nla = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
  // 3 nest peer
  struct nlattr *peer = mnl_attr_nest_start(nlh, VETH_INFO_PEER);

  enterifi(nlh);
  mnl_attr_put_strz(nlh, IFLA_IFNAME, out_name);

  mnl_attr_nest_end(nlh, peer);
  mnl_attr_nest_end(nlh, nla);
  mnl_attr_nest_end(nlh, li);

  return mnl_ack(nl, nlh, buf, sizeof(buf));
}

// move iface "ifname" to ns "ns"
int if_moveto(struct mnl_socket *nl, const char *ifname, int ns) {
  char buf[MNL_SKBUFSZ];
  memset(buf, 0, sizeof(buf));

  struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
  nlh->nlmsg_type = RTM_NEWLINK;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  nlh->nlmsg_seq = next_seq();

  struct ifinfomsg *ifi = enterifi(nlh);
  ifi->ifi_index = (int)if_nametoindex(ifname);

  if (ifi->ifi_index == 0) {
    return -ENODEV; // interface not found in ns
  }

  mnl_attr_put_u32(nlh, IFLA_NET_NS_FD, (uint32_t)ns);
  return mnl_ack(nl, nlh, buf, sizeof(buf));
}

// up interface in current ns
int if_up(struct mnl_socket *nl, const char *ifname) {
  char buf[MNL_SKBUFSZ];
  memset(buf, 0, sizeof(buf));

  struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
  nlh->nlmsg_type = RTM_NEWLINK;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  nlh->nlmsg_seq = next_seq();

  // use helper but change the fields
  struct ifinfomsg *ifi = enterifi(nlh);
  ifi->ifi_index = (int)if_nametoindex(ifname);
  if (ifi->ifi_index == 0)
    return -ENODEV;

  ifi->ifi_flags = IFF_UP;
  ifi->ifi_change = IFF_UP;

  return mnl_ack(nl, nlh, buf, sizeof(buf));
}

// add ip/prefix addr to interface
int if_add_addr(struct mnl_socket *nl, const char *ifname, const char *ip,
                uint8_t prefix) {
  char buf[MNL_SKBUFSZ];
  memset(buf, 0, sizeof(buf));

  struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
  nlh->nlmsg_type = RTM_NEWADDR;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE |
                     NLM_F_REPLACE; // replace if exist
  nlh->nlmsg_seq = next_seq();

  int ifidx = (int)if_nametoindex(ifname);
  if (ifidx == 0) {
    return -ENODEV;
  }

  struct ifaddrmsg *ifa = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifa));
  ifa->ifa_family = AF_INET;
  ifa->ifa_prefixlen = prefix;
  ifa->ifa_index = (uint32_t)ifidx;
  ifa->ifa_scope = 0;

  struct in_addr a;
  inet_pton(AF_INET, ip, &a);

  mnl_attr_put(nlh, IFA_LOCAL, sizeof(a), &a);
  mnl_attr_put(nlh, IFA_ADDRESS, sizeof(a), &a);

  return mnl_ack(nl, nlh, buf, sizeof(buf));
}

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
