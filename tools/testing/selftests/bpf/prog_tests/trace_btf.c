// SPDX-License-Identifier: GPL-2.0
#include <test_progs.h>

#include "netif_receive_skb.skel.h"

void test_trace_btf(void)
{
	struct netif_receive_skb *skel;
	struct netif_receive_skb__bss *bss;
	int err, duration = 0;

	skel = netif_receive_skb__open();
	if (CHECK(!skel, "skel_open", "failed to open skeleton\n"))
		return;

	err = netif_receive_skb__load(skel);
	if (CHECK(err, "skel_load", "failed to load skeleton: %d\n", err))
		goto cleanup;

	bss = skel->bss;

	err = netif_receive_skb__attach(skel);
	if (CHECK(err, "skel_attach", "skeleton attach failed: %d\n", err))
		goto cleanup;

	/* generate receive event */
	system("ping -c 10 127.0.0.1");

	/*
	 * Make sure netif_receive_skb program was triggered
	 * and it set expected return values from bpf_trace_printk()s
	 * and all tests ran.
	 */
	if (CHECK(bss->ret <= 0,
		  "bpf_trace_btf: got return value",
		  "ret <= 0 %ld test %d\n", bss->ret, bss->num_subtests))
		goto cleanup;

	CHECK(bss->num_subtests != bss->ran_subtests, "check all subtests ran",
	      "only ran %d of %d tests\n", bss->num_subtests,
	      bss->ran_subtests);

cleanup:
	netif_receive_skb__destroy(skel);
}
