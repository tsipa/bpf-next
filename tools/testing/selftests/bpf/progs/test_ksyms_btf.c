// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Google */

#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

__u64 out__runqueues = -1;
__u64 out__bpf_prog_active = -1;
__u32 out__rq_cpu = -1;
unsigned long out__process_counts = -1;

extern const struct rq runqueues __ksym; /* struct type percpu var. */
extern const int bpf_prog_active __ksym; /* int type global var. */
extern const unsigned long process_counts __ksym; /* int type percpu var. */

SEC("raw_tp/sys_enter")
int handler(const void *ctx)
{
	struct rq *rq;
	unsigned long *count;

	out__runqueues = (__u64)&runqueues;
	out__bpf_prog_active = (__u64)&bpf_prog_active;

	rq = (struct rq *)bpf_per_cpu_ptr(&runqueues, 1);
	if (rq)
		out__rq_cpu = rq->cpu;
	count = (unsigned long *)bpf_per_cpu_ptr(&process_counts, 1);
	if (count)
		out__process_counts = *count;

	return 0;
}

char _license[] SEC("license") = "GPL";
