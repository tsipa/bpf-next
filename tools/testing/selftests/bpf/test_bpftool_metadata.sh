#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

# Kselftest framework requirement - SKIP code is 4.
ksft_skip=4

TESTNAME=bpftool_metadata
BPF_FS=$(awk '$3 == "bpf" {print $2; exit}' /proc/mounts)
BPF_DIR=$BPF_FS/test_$TESTNAME

_cleanup()
{
	set +e
	rm -rf $BPF_DIR 2> /dev/null
}

cleanup_skip()
{
	echo "selftests: $TESTNAME [SKIP]"
	_cleanup

	exit $ksft_skip
}

cleanup()
{
	if [ "$?" = 0 ]; then
		echo "selftests: $TESTNAME [PASS]"
	else
		echo "selftests: $TESTNAME [FAILED]"
	fi
	_cleanup
}

if [ $(id -u) -ne 0 ]; then
	echo "selftests: $TESTNAME [SKIP] Need root privileges"
	exit $ksft_skip
fi

if [ -z "$BPF_FS" ]; then
	echo "selftests: $TESTNAME [SKIP] Could not run test without bpffs mounted"
	exit $ksft_skip
fi

if ! bpftool version > /dev/null 2>&1; then
	echo "selftests: $TESTNAME [SKIP] Could not run test without bpftool"
	exit $ksft_skip
fi

set -e

trap cleanup_skip EXIT

mkdir $BPF_DIR

trap cleanup EXIT

bpftool prog load metadata_unused.o $BPF_DIR/unused

METADATA_PLAIN="$(bpftool prog --metadata)"
echo "$METADATA_PLAIN" | grep 'metadata_a = "foo"' > /dev/null
echo "$METADATA_PLAIN" | grep 'metadata_b = 1' > /dev/null

bpftool prog --metadata --json | grep '"metadata":{"metadata_a":"foo","metadata_b":1}' > /dev/null

bpftool map | grep 'metada.metadata' > /dev/null

rm $BPF_DIR/unused

bpftool prog load metadata_used.o $BPF_DIR/used

METADATA_PLAIN="$(bpftool prog --metadata)"
echo "$METADATA_PLAIN" | grep 'metadata_a = "bar"' > /dev/null
echo "$METADATA_PLAIN" | grep 'metadata_b = 2' > /dev/null

bpftool prog --metadata --json | grep '"metadata":{"metadata_a":"bar","metadata_b":2}' > /dev/null

bpftool map | grep 'metada.metadata' > /dev/null

rm $BPF_DIR/used

exit 0
