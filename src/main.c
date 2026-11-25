#include "ns.h"
#include <libmnl/libmnl.h>

int main(void) {
  int f;
  // init socket
  struct mnl_socket *main = initmnl();
  if (main == NULL) {
    return -1;
  }

  // 1. declare structures
  // client
  struct node n = {.name = "client"};
  if (node_apply(&n) < 0) {
    puts("cannot make node from struct");
    return -1;
  }

  // server
  struct node m = {.name = "server"};
  if (node_apply(&m) < 0) {
    puts("cannot make node from struct");
    return -1;
  }

  // router
  struct node r = {.name = "router"};
  if (node_apply(&r) < 0) {
    return -1;
  }

  // describe veth iface structures
  struct iface i0 = {.name = "veth0"};
  struct iface i1 = {.name = "veth1"};
  struct iface r_in = {.name = "r0"};
  struct iface r_out = {.name = "r1"};

  // 2. apply this to kernel

  // create our links
  veth_pair(main, r_in.name, i0.name);
  veth_pair(main, r_out.name, i1.name);

  // move link ends to nodes
  if_moveto(main, i0.name, n.fd);
  if_moveto(main, i1.name, m.fd);
  if_moveto(main, r_in.name, r.fd);
  if_moveto(main, r_out.name, r.fd);

  // setup interfaces from nses
  // client
  f = ns_enter(n.fd);
  struct mnl_socket *nl = initmnl();
  if (if_add_addr(nl, i0.name, "10.0.0.1", 24) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, i0.name) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, "lo") < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (ns_add_default_route(nl, i0.name, "10.0.0.254") < 0) {
    mnl_socket_close(nl);
    return -1;
  }
  mnl_socket_close(nl);
  ns_exit(f);

  // router
  f = ns_enter(r.fd);
  nl = initmnl();
  if (if_add_addr(nl, r_in.name, "10.0.0.254", 24) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, r_in.name) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, "lo") < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_add_addr(nl, r_out.name, "10.0.2.254", 24) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, r_out.name) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (ns_ip_fwd() < 0) {
    puts("set ip forward");
    return -1;
  };

  ns_exit(f);

  // server
  f = ns_enter(m.fd);
  nl = initmnl();
  if (if_add_addr(nl, i1.name, "10.0.2.1", 24) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, i1.name) < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (if_up(nl, "lo") < 0) {
    mnl_socket_close(nl);
    return -1;
  }

  if (ns_add_default_route(nl, i1.name, "10.0.2.254") < 0) {
    mnl_socket_close(nl);
    return -1;
  }
  mnl_socket_close(nl);
  ns_exit(f);

  // 3. exec testing programs
  do_traceroute(&n, "10.0.2.1");
  return 0;
}
