#include "ns.h"

// public router - no nat, no fw - just ip_forward
int preset_pub(struct node *n, const char *wan_if) {
  int ret = sysctl_write("/proc/sys/net/ipv4/ip_forward", "1");

  return ret;
}
