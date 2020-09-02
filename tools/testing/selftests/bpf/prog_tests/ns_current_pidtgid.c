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

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

void test_ns_current_pid_tgid_global_ns(void)
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

static int newns_pidtgid(void *arg)
{
	struct test_ns_current_pidtgid__bss  *bss;
	int pidns_fd = 0, err = 0, duration = 0;
	struct test_ns_current_pidtgid *skel;
	pid_t pid, tid;
	struct stat st;
	__u64 id;

	skel = test_ns_current_pidtgid__open_and_load();
	if (!skel) {
		perror("Failed to load skeleton");
		goto cleanup;
	}

	tid = syscall(SYS_gettid);
	pid = getpid();
	id = ((__u64) tid << 32) | pid;

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
	setns(pidns_fd, CLONE_NEWPID);
	test_ns_current_pidtgid__destroy(skel);

	return err;
}

void test_ns_current_pid_tgid_new_ns(void)
{
	int wstatus, duration = 0;
	pid_t cpid;

	cpid = clone(newns_pidtgid,
			child_stack + STACK_SIZE,
			CLONE_NEWPID | SIGCHLD, NULL);

	if (CHECK(cpid == -1, "clone",
		strerror(errno))) {
		exit(EXIT_FAILURE);
	}

	if (CHECK(waitpid(cpid, &wstatus, 0) == -1, "waitpid",
		strerror(errno))) {
		exit(EXIT_FAILURE);
	}

	CHECK(WEXITSTATUS(wstatus) != 0, "newns_pidtgid",
		"failed");
}

void test_ns_current_pidtgid(void)
{
	if (test__start_subtest("ns_current_pid_tgid_global_ns"))
		test_ns_current_pid_tgid_global_ns();
	if (test__start_subtest("ns_current_pid_tgid_new_ns"))
		test_ns_current_pid_tgid_new_ns();
}
