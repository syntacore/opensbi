
asn1-srcs := $(wildcard lib/utils/ud/generated/*.c)
asn1-headers := $(wildcard lib/utils/ud/generated/*.h)
asn1-objs := $(patsubst lib/utils/ud/generated/%.c,ud/generated/%.o,$(asn1-srcs))

libsbiutils-objs-$(CONFIG_UNIFIED_DISCOVERY) += ud/ud_helper.o
libsbiutils-objs-$(CONFIG_UNIFIED_DISCOVERY) += ud/errno.o
libsbiutils-objs-$(CONFIG_UNIFIED_DISCOVERY) += ud/basic_allocator.o
libsbiutils-objs-$(CONFIG_UNIFIED_DISCOVERY) += ud/ud_extension_mapping.o
libsbiutils-objs-$(CONFIG_UNIFIED_DISCOVERY) += $(asn1-objs)
libsbiutils-genflags-$(CONFIG_UNIFIED_DISCOVERY) += -I$(libsbiutils_dir)/ud/generated/
