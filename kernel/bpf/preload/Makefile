# SPDX-License-Identifier: GPL-2.0

LIBBPF := $(srctree)/../../tools/lib/bpf
userccflags += -I $(srctree)/tools/include/ -I $(srctree)/tools/include/uapi -I $(LIBBPF) \
	-I $(srctree)/tools/lib/ \
	-I $(srctree)/kernel/bpf/preload/iterators/ -Wno-int-conversion \
	-DCOMPAT_NEED_REALLOCARRAY

userprogs := bpf_preload_umd

LIBBPF_O := $(LIBBPF)/bpf.o $(LIBBPF)/libbpf.o $(LIBBPF)/btf.o $(LIBBPF)/libbpf_errno.o \
	$(LIBBPF)/str_error.o $(LIBBPF)/hashmap.o $(LIBBPF)/libbpf_probes.o

bpf_preload_umd-objs := iterators/iterators.o $(LIBBPF_O)

userldflags += -lelf -lz

$(obj)/bpf_preload_umd_blob.o: $(obj)/bpf_preload_umd

obj-$(CONFIG_BPF_PRELOAD_UMD) += bpf_preload.o
bpf_preload-objs += bpf_preload_kern.o bpf_preload_umd_blob.o
