// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Google */

#include <test_progs.h>
#include <bpf/libbpf.h>
#include <bpf/btf.h>
#include "test_ksyms_btf.skel.h"

static int duration;

static __u64 kallsyms_find(const char *sym)
{
	char type, name[500];
	__u64 addr, res = 0;
	FILE *f;

	f = fopen("/proc/kallsyms", "r");
	if (CHECK(!f, "kallsyms_fopen", "failed to open: %d\n", errno))
		return 0;

	while (fscanf(f, "%llx %c %499s%*[^\n]\n", &addr, &type, name) > 0) {
		if (strcmp(name, sym) == 0) {
			res = addr;
			goto out;
		}
	}

	CHECK(false, "not_found", "symbol %s not found\n", sym);
out:
	fclose(f);
	return res;
}

void test_ksyms_btf(void)
{
	__u64 runqueues_addr = kallsyms_find("runqueues");
	__u64 bpf_prog_active_addr = kallsyms_find("bpf_prog_active");
	struct test_ksyms_btf *skel;
	struct test_ksyms_btf__data *data;
	struct btf *btf;
	int percpu_datasec;
	int err;

	btf = libbpf_find_kernel_btf();
	if (CHECK(IS_ERR(btf), "btf_exists", "failed to load kernel BTF: %ld\n",
		  PTR_ERR(btf)))
		return;

	percpu_datasec = btf__find_by_name_kind(btf, ".data..percpu",
						BTF_KIND_DATASEC);
	if (percpu_datasec < 0) {
		printf("%s:SKIP:no PERCPU DATASEC in kernel btf\n",
		       __func__);
		test__skip();
		return;
	}

	skel = test_ksyms_btf__open_and_load();
	if (CHECK(!skel, "skel_open", "failed to open and load skeleton\n"))
		return;

	err = test_ksyms_btf__attach(skel);
	if (CHECK(err, "skel_attach", "skeleton attach failed: %d\n", err))
		goto cleanup;

	/* trigger tracepoint */
	usleep(1);

	data = skel->data;
	CHECK(data->out__runqueues != runqueues_addr, "runqueues",
	      "got %llu, exp %llu\n", data->out__runqueues, runqueues_addr);
	CHECK(data->out__bpf_prog_active != bpf_prog_active_addr, "bpf_prog_active",
	      "got %llu, exp %llu\n", data->out__bpf_prog_active, bpf_prog_active_addr);
	CHECK(data->out__rq_cpu != 1, "rq_cpu",
	      "got %u, exp %u\n", data->out__rq_cpu, 1);
	CHECK(data->out__process_counts == -1, "process_counts",
	      "got %lu, exp != -1", data->out__process_counts);

cleanup:
	test_ksyms_btf__destroy(skel);
}
