// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020, Oracle and/or its affiliates. */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char _license[] SEC("license") = "GPL";

long ret = 0;
int num_subtests = 0;
int ran_subtests = 0;

#define CHECK_TRACE(_p, flags)						 \
	do {								 \
		++num_subtests;						 \
		if (ret >= 0) {						 \
			++ran_subtests;					 \
			ret = bpf_trace_btf(_p, sizeof(*(_p)), 0, flags);\
		}							 \
	} while (0)

/* TRACE_EVENT(netif_receive_skb,
 *	TP_PROTO(struct sk_buff *skb),
 */
SEC("tp_btf/netif_receive_skb")
int BPF_PROG(trace_netif_receive_skb, struct sk_buff *skb)
{
	static const char skb_type[] = "struct sk_buff";
	static struct btf_ptr p = { };

	p.ptr = skb;
	p.type = skb_type;

	CHECK_TRACE(&p, 0);
	CHECK_TRACE(&p, BTF_TRACE_F_COMPACT);
	CHECK_TRACE(&p, BTF_TRACE_F_NONAME);
	CHECK_TRACE(&p, BTF_TRACE_F_PTR_RAW);
	CHECK_TRACE(&p, BTF_TRACE_F_ZERO);
	CHECK_TRACE(&p, BTF_TRACE_F_COMPACT | BTF_TRACE_F_NONAME |
		    BTF_TRACE_F_PTR_RAW | BTF_TRACE_F_ZERO);

	return 0;
}
