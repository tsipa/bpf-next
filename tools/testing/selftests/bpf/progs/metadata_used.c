// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

char metadata_a[] SEC(".metadata") = "bar";
int metadata_b SEC(".metadata") = 2;

SEC("cgroup_skb/egress")
int prog(struct xdp_md *ctx)
{
	return metadata_b ? 1 : 0;
}

char _license[] SEC("license") = "GPL";
