#
# brtclite Makefile
#
#
#

TARGET = brtclite
SOTARGET = libbaidurtc_lite
SOSUFFIX = so

CPU_ARCH = $(shell uname -m)

$(info  Host CPU ARCH: $(CPU_ARCH))

ifeq ($(BRTC_BUILD_ARCH), armv7-softfp)
BRTC_BUILD_ARM_EABI_SOFTFP = yes
else ifeq ($(BRTC_BUILD_ARCH), armv7)
BRTC_BUILD_ARM = yes
else ifeq ($(BRTC_BUILD_ARCH), arm64)
BRTC_BUILD_ARM64 = yes
else ifeq ($(BRTC_BUILD_ARCH), armv7-softfp-uclibc)
BRTC_BUILD_ARM_EABI_SOFTFP_UCLIBC = yes
else ifeq ($(BRTC_BUILD_ARCH), armv7-soft-uclibc)
BRTC_BUILD_ARM_EABI_SOFT_UCLIBC = yes
else ifeq ($(BRTC_BUILD_ARCH), qnx-arm64)
BRTC_BUILD_QNX_ARM64 = yes
else ifeq ($(BRTC_BUILD_ARCH), qnx-armv7)
BRTC_BUILD_QNX_ARM = yes
else ifeq ($(BRTC_BUILD_ARCH), qnx-x86)
BRTC_BUILD_QNX_X86 = yes
else ifeq ($(BRTC_BUILD_ARCH), qnx-x86_64)
BRTC_BUILD_QNX_X86_64 = yes
endif

GCC_PREFIX = 

ARM_EABI_SOFTFP =
ifeq ($(BRTC_BUILD_ARM_EABI_SOFTFP), yes)
CPU_ARCH = armv7l
ARM_EABI_SOFTFP = -softfp
GCC_PREFIX = arm-linux-gnueabi-
endif

ifeq ($(BRTC_BUILD_ARM), yes)
CPU_ARCH = armv7l
GCC_PREFIX = arm-linux-gnueabihf-
endif

ifeq ($(BRTC_BUILD_ARM64), yes)
CPU_ARCH = aarch64
GCC_PREFIX = aarch64-linux-gnu-
endif

ifeq ($(BRTC_BUILD_ARM_EABI_SOFTFP_UCLIBC), yes)
CPU_ARCH = armv7l
ARM_EABI_SOFTFP = -softfp-uclibc
GCC_PREFIX = arm-hisiv500-linux-uclibcgnueabi-
endif

ifeq ($(BRTC_BUILD_ARM_EABI_SOFT_UCLIBC), yes)
CPU_ARCH = armv7l
ARM_EABI_SOFTFP = -soft-uclibc
GCC_PREFIX = arm-hisiv500-linux-uclibcgnueabi-
endif

ifeq ($(BRTC_BUILD_QNX_ARM64), yes)
CPU_TYPE = arm64
GCC_PREFIX = aarch64-unknown-nto-qnx7.0.0-
CPU_ARCH = qnx-cpu-arch
else ifeq ($(BRTC_BUILD_QNX_ARM), yes)
CPU_TYPE = armv7
GCC_PREFIX = arm-unknown-nto-qnx7.0.0eabi-
CPU_ARCH = qnx-cpu-arch
else ifeq ($(BRTC_BUILD_QNX_X86), yes)
CPU_TYPE = x86
GCC_PREFIX = i586-pc-nto-qnx7.0.0-
CPU_ARCH = qnx-cpu-arch
else ifeq ($(BRTC_BUILD_QNX_X86_64), yes)
CPU_TYPE = x86_64
GCC_PREFIX = x86_64-pc-nto-qnx7.0.0-
CPU_ARCH = qnx-cpu-arch
endif

$(info  Set target CPU ARCH to: $(CPU_ARCH) $(CPU_TYPE))


brtclite:
	$(MAKE) -C re install ARCH=$(CPU_ARCH)
	$(MAKE) -C rem install ARCH=$(CPU_ARCH)
	$(MAKE) -C baresip install install-dev ARCH=$(CPU_ARCH)
	$(MAKE) -C barertc install ARCH=$(CPU_ARCH)

clean:
	$(MAKE) -C re clean ARCH=$(CPU_ARCH)
	$(MAKE) -C rem clean ARCH=$(CPU_ARCH)
	$(MAKE) -C baresip clean ARCH=$(CPU_ARCH)
	$(MAKE) -C barertc clean ARCH=$(CPU_ARCH)

default: brtclite

all: default

rebuild: clean default
