// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 Cloudflare
#include <error.h>

#include "test_progs.h"
#include "test_skmsg_load_helpers.skel.h"
#include "test_sockmap_copy.skel.h"

#define TCP_REPAIR		19	/* TCP sock is under repair right now */

#define TCP_REPAIR_ON		1
#define TCP_REPAIR_OFF_NO_WP	-1	/* Turn off without window probes */

static int connected_socket_v4(void)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(80),
		.sin_addr = { inet_addr("127.0.0.1") },
	};
	socklen_t len = sizeof(addr);
	int s, repair, err;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (CHECK_FAIL(s == -1))
		goto error;

	repair = TCP_REPAIR_ON;
	err = setsockopt(s, SOL_TCP, TCP_REPAIR, &repair, sizeof(repair));
	if (CHECK_FAIL(err))
		goto error;

	err = connect(s, (struct sockaddr *)&addr, len);
	if (CHECK_FAIL(err))
		goto error;

	repair = TCP_REPAIR_OFF_NO_WP;
	err = setsockopt(s, SOL_TCP, TCP_REPAIR, &repair, sizeof(repair));
	if (CHECK_FAIL(err))
		goto error;

	return s;
error:
	perror(__func__);
	close(s);
	return -1;
}

/* Create a map, populate it with one socket, and free the map. */
static void test_sockmap_create_update_free(enum bpf_map_type map_type)
{
	const int zero = 0;
	int s, map, err;

	s = connected_socket_v4();
	if (CHECK_FAIL(s == -1))
		return;

	map = bpf_create_map(map_type, sizeof(int), sizeof(int), 1, 0);
	if (CHECK_FAIL(map == -1)) {
		perror("bpf_create_map");
		goto out;
	}

	err = bpf_map_update_elem(map, &zero, &s, BPF_NOEXIST);
	if (CHECK_FAIL(err)) {
		perror("bpf_map_update");
		goto out;
	}

out:
	close(map);
	close(s);
}

static void test_skmsg_helpers(enum bpf_map_type map_type)
{
	struct test_skmsg_load_helpers *skel;
	int err, map, verdict;

	skel = test_skmsg_load_helpers__open_and_load();
	if (CHECK_FAIL(!skel)) {
		perror("test_skmsg_load_helpers__open_and_load");
		return;
	}

	verdict = bpf_program__fd(skel->progs.prog_msg_verdict);
	map = bpf_map__fd(skel->maps.sock_map);

	err = bpf_prog_attach(verdict, map, BPF_SK_MSG_VERDICT, 0);
	if (CHECK_FAIL(err)) {
		perror("bpf_prog_attach");
		goto out;
	}

	err = bpf_prog_detach2(verdict, map, BPF_SK_MSG_VERDICT);
	if (CHECK_FAIL(err)) {
		perror("bpf_prog_detach2");
		goto out;
	}
out:
	test_skmsg_load_helpers__destroy(skel);
}

static void test_sockmap_copy(enum bpf_map_type map_type)
{
	struct bpf_prog_test_run_attr attr;
	struct test_sockmap_copy *skel;
	__u64 src_cookie, dst_cookie;
	int err, prog, s, src, dst;
	const __u32 zero = 0;
	char dummy[14] = {0};

	s = connected_socket_v4();
	if (CHECK_FAIL(s == -1))
		return;

	skel = test_sockmap_copy__open_and_load();
	if (CHECK_FAIL(!skel)) {
		close(s);
		perror("test_sockmap_copy__open_and_load");
		return;
	}

	prog = bpf_program__fd(skel->progs.copy_sock_map);
	src = bpf_map__fd(skel->maps.src);
	if (map_type == BPF_MAP_TYPE_SOCKMAP)
		dst = bpf_map__fd(skel->maps.dst_sock_map);
	else
		dst = bpf_map__fd(skel->maps.dst_sock_hash);

	err = bpf_map_update_elem(src, &zero, &s, BPF_NOEXIST);
	if (CHECK_FAIL(err)) {
		perror("bpf_map_update");
		goto out;
	}

	err = bpf_map_lookup_elem(src, &zero, &src_cookie);
	if (CHECK_FAIL(err)) {
		perror("bpf_map_lookup_elem(src)");
		goto out;
	}

	attr = (struct bpf_prog_test_run_attr){
		.prog_fd = prog,
		.repeat = 1,
		.data_in = dummy,
		.data_size_in = sizeof(dummy),
	};

	err = bpf_prog_test_run_xattr(&attr);
	if (err) {
		test__fail();
		perror("bpf_prog_test_run");
		goto out;
	} else if (!attr.retval) {
		PRINT_FAIL("bpf_prog_test_run: program returned %u\n",
			   attr.retval);
		goto out;
	}

	err = bpf_map_lookup_elem(dst, &zero, &dst_cookie);
	if (CHECK_FAIL(err)) {
		perror("bpf_map_lookup_elem(dst)");
		goto out;
	}

	if (dst_cookie != src_cookie)
		PRINT_FAIL("cookie %llu != %llu\n", dst_cookie, src_cookie);

out:
	close(s);
	test_sockmap_copy__destroy(skel);
}

void test_sockmap_basic(void)
{
	if (test__start_subtest("sockmap create_update_free"))
		test_sockmap_create_update_free(BPF_MAP_TYPE_SOCKMAP);
	if (test__start_subtest("sockhash create_update_free"))
		test_sockmap_create_update_free(BPF_MAP_TYPE_SOCKHASH);
	if (test__start_subtest("sockmap sk_msg load helpers"))
		test_skmsg_helpers(BPF_MAP_TYPE_SOCKMAP);
	if (test__start_subtest("sockhash sk_msg load helpers"))
		test_skmsg_helpers(BPF_MAP_TYPE_SOCKHASH);
	if (test__start_subtest("sockmap copy"))
		test_sockmap_copy(BPF_MAP_TYPE_SOCKMAP);
	if (test__start_subtest("sockhash copy"))
		test_sockmap_copy(BPF_MAP_TYPE_SOCKHASH);
}
