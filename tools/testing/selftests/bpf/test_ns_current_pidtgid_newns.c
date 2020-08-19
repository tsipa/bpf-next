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

static int newns_pidtgid(void *arg)
{
	struct test_ns_current_pidtgid__bss  *bss;
	struct test_ns_current_pidtgid *skel;
	int pidns_fd = 0, err = 0;
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

	if (stat("/proc/self/ns/pid", &st)) {
		printf("Failed to stat /proc/self/ns/pid: %s\n",
			strerror(errno));
		goto cleanup;
	}

	bss = skel->bss;
	bss->dev = st.st_dev;
	bss->ino = st.st_ino;
	bss->user_pid_tgid = 0;

	err = test_ns_current_pidtgid__attach(skel);
	if (err) {
		printf("Failed to attach: %s err: %d\n", strerror(errno), err);
		goto cleanup;
	}
	/* trigger tracepoint */
	usleep(1);

	if (bss->user_pid_tgid != id) {
		printf("test_ns_current_pidtgid_newns:FAIL\n");
		err = EXIT_FAILURE;
	} else {
		printf("test_ns_current_pidtgid_newns:PASS\n");
		err = EXIT_SUCCESS;
	}

cleanup:
	setns(pidns_fd, CLONE_NEWPID);
	test_ns_current_pidtgid__destroy(skel);

	return err;
}

int main(int argc, char **argv)
{
	pid_t cpid;
	int wstatus;

	cpid = clone(newns_pidtgid,
			child_stack + STACK_SIZE,
			CLONE_NEWPID | SIGCHLD, NULL);
	if (cpid == -1) {
		printf("test_ns_current_pidtgid_newns:Failed on CLONE: %s\n",
			 strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (waitpid(cpid, &wstatus, 0) == -1) {
		printf("test_ns_current_pidtgid_newns:Failed on waitpid: %s\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	return WEXITSTATUS(wstatus);
}
