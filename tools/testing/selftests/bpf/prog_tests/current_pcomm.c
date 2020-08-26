// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Carlos Neira cneirabustos@gmail.com */

#define _GNU_SOURCE
#include <test_progs.h>
#include "test_current_pcomm.skel.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

void *current_pcomm(void *args)
{
	struct test_current_pcomm__bss  *bss;
	struct test_current_pcomm *skel;
	int err, duration = 0;

	skel = test_current_pcomm__open_and_load();
	if (CHECK(!skel, "skel_open_load", "failed to load skeleton"))
		goto cleanup;

	bss = skel->bss;

	err = test_current_pcomm__attach(skel);
	if (CHECK(err, "skel_attach", "skeleton attach failed %d", err))
		goto cleanup;

	/* trigger tracepoint */
	usleep(10);
	err = memcmp(bss->comm, "current_pcomm2", 14);
	if (CHECK(!err, "pcomm ", "bss->comm: %s\n", bss->comm))
		goto cleanup;
cleanup:
	test_current_pcomm__destroy(skel);
	return NULL;
}

int test_current_pcomm(void)
{
	int err = 0, duration = 0;
	pthread_t tid;

	err = pthread_create(&tid, NULL, &current_pcomm, NULL);
	if (CHECK(err, "thread", "thread creation failed %d", err))
		return EXIT_FAILURE;
	err = pthread_setname_np(tid, "current_pcomm2");
	if (CHECK(err, "thread naming", "thread naming failed %d", err))
		return EXIT_FAILURE;

	usleep(5);

	err = pthread_join(tid, NULL);
	if (CHECK(err, "thread join", "thread join failed %d", err))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
