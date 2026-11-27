#pragma once

// shared header for all .c , will be splitted into internal/external soon

#include <net/if.h>
#include <stdint.h>

#include <libmnl/libmnl.h>

#define NODE_NAMESIZE 64
#define MNL_SKBUFSZ                                                            \
  2048 // MNL_SKBUFSZ is macro so i can't use vla for stack arrays

// node for net graph
struct node {
  char name[NODE_NAMESIZE]; // for log/debug

  int fd;               // namespace fd
  struct iface *ifs[8]; // 8 for mvp / linked list for project
  size_t n_if;
};

struct iface {
  char name[IFNAMSIZ];
  uint32_t ip; // ip in network byte order
  uint8_t prefix;
  struct node *n;
};

struct link {
  struct iface *a;
  struct iface *b;

  char type;
};

struct mnl_socket *initmnl();
int node_apply(struct node *n);

int ns_enter(int to);
int ns_exit(int to);
int sysctl_write(const char *to, const char *buf);
int ns_add_default_route(struct mnl_socket *nl, const char *oif_name,
                         const char *gw_ip);

int veth_pair(struct mnl_socket *nl, const char *in_name, const char *out_name);

int if_moveto(struct mnl_socket *nl, const char *ifname, int ns);
int if_up(struct mnl_socket *nl, const char *ifname);
int if_add_addr(struct mnl_socket *nl, const char *ifname, const char *ip,
                uint8_t prefix);

int ns_exec(const int ns, const char *argv[]);
int do_ping(const int fd, const char *toip);
int do_traceroute(const int fd, const char *toip);

int mnl_ack(struct mnl_socket *nl, struct nlmsghdr *nlh, char *buf,
            const size_t buflen);
struct ifinfomsg *enterifi(struct nlmsghdr *nlh);

extern uint32_t seq;
uint32_t next_seq();
