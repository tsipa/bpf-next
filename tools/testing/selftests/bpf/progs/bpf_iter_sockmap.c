// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Cloudflare */
#include "bpf_iter.h"
#include "bpf_tracing_net.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char _license[] SEC("license") = "GPL";

struct {
	__uint(type, BPF_MAP_TYPE_SOCKMAP);
	__uint(max_entries, 3);
	__type(key, __u32);
	__type(value, __u64);
} sockmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_SOCKMAP);
	__uint(max_entries, 3);
	__type(key, __u32);
	__type(value, __u64);
} sockhash SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_SOCKHASH);
	__uint(max_entries, 3);
	__type(key, __u32);
	__type(value, __u64);
} dst SEC(".maps");

__u32 elems = 0;

SEC("iter/sockmap")
int copy_sockmap(struct bpf_iter__sockmap *ctx)
{
	__u32 tmp, *key = ctx->key;
	struct bpf_sock *sk = ctx->sk;

	if (key == (void *)0)
		return 0;

	elems++;
	tmp = *key;

	if (sk != (void *)0)
		return bpf_map_update_elem(&dst, &tmp, sk, 0) != 0;

	bpf_map_delete_elem(&dst, &tmp);
	return 0;
}
