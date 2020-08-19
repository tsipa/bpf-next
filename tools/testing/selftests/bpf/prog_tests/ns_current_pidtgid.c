// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Carlos Neira cneirabustos@gmail.com */

#define _GNU_SOURCE
#include <test_progs.h>
#include "test_ns_current_pidtgid.skel.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/fcntl.h>

void test_ns_current_pidtgid(void)
{
	struct test_ns_current_pidtgid__bss  *bss;
	struct test_ns_current_pidtgid *skel;
	int err, duration = 0;
	struct stat st;
	pid_t tid, pid;
	__u64 id;

	skel = test_ns_current_pidtgid__open_and_load();
	CHECK(!skel, "skel_open_load", "failed to load skeleton\n");
		goto cleanup;

	tid = syscall(SYS_gettid);
	pid = getpid();

	id = ((__u64)tid << 32) | pid;

	err = stat("/proc/self/ns/pid", &st);
	if (CHECK(err, "stat", "failed /proc/self/ns/pid: %d", err))
		goto cleanup;

	bss = skel->bss;
	bss->dev = st.st_dev;
	bss->ino = st.st_ino;
	bss->user_pid_tgid = 0;

	err = test_ns_current_pidtgid__attach(skel);
	if (CHECK(err, "skel_attach", "skeleton attach failed: %d\n", err))
		goto cleanup;

	/* trigger tracepoint */
	usleep(1);

	CHECK(bss->user_pid_tgid != id, "pid/tgid", "got %llu != exp %llu\n",
		bss->user_pid_tgid, id);
cleanup:
	test_ns_current_pidtgid__destroy(skel);

}
