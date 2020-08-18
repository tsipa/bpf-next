// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

char metadata_a[] SEC(".metadata") = "foo";
int metadata_b SEC(".metadata") = 1;

SEC("cgroup_skb/egress")
int prog(struct xdp_md *ctx)
{
	return 0;
}

char _license[] SEC("license") = "GPL";
