// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Carlos Neira cneirabustos@gmail.com */

#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>

char comm[16] = {0};

SEC("raw_tracepoint/sys_enter")
int current_pcomm(const void *ctx)
{
	bpf_get_current_pcomm(comm, sizeof(comm));
	return 0;
}

char _license[] SEC("license") = "GPL";
