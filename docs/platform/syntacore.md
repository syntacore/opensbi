Syntacore Platform
=========================
Syntacore platform build is used for various RISC-V cores (32-bit and 64-bit).
The Syntacore development platform is capable of running Linux.

With QEMU from Syntacore development toolkit (sc-dt), the 'scrX' machine
('X' here is cpu model) software for Syntacore platforms can be launched.
Syntacore development toolkit can be found on our website:
https://syntacore.com/tools/development-tools

Building Syntacore Platform
------------------------------

Our bootchain looks like this:

```
Miniboot->OpenSBI->U-Boot->Linux
```

So in our situation OpenSBI launches after primary settings are made by
Miniboot. Miniboot is the proprietary first-stage bootloader developed by
Syntacore for its clusters. It is executing prior to OpenSBI to perform
initial hardware configuration.
More information about Miniboot and source code can be found here:
https://github.com/syntacore/miniboot

We can build 32-bit and 64-bit configurations. Build string will look like
in example below.

For 64-bit platforms:
```
make CROSS_COMPILE="${SC_GCC_PATH}/bin/riscv64-unknown-linux-gnu-" \
PLATFORM=generic O=build
```
For 32-bit platforms:
```
make CROSS_COMPILE="${SC_GCC_PATH}/bin/riscv64-unknown-linux-gnu-" \
PLATFORM=generic PLATFORM_RISCV_XLEN=32 O=build
```

Here SC_GCC_PATH is a path to Syntacore riscv-gcc toolchain.
We can also use LLVM from Syntacore development toolkit. In this case,
build string should contain path to LLVM (SC_LLVM_PATH) instead of SC_GCC_PATH:
```
make LLVM="${SC_LLVM_PATH}/bin/" PLATFORM=generic O=build
```

Syntacore specific dts nodes
---------------------------
Syntacore device trees have their specific nodes based on which OpenSBI performs
platform specific settings.
Nodes with their description are listed below.

MPU node declares that using MPU protection instead of PMP. We don't need
any information inside this node, parser is loooking only for name:
```
/{
	chosen {
		opensbi-domains {

			scr_mpu: scr_mpu {
			};
		};
	};
};
```

ICCM (old) node (used in L2 platforms):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			iccm: iccm {
				compatible = "syntacore,iccm";
				mbox-mapping = <&cpu0 0 &cpu1 1
						&cpu2 2 &cpu3 3>;
			};
		};
	};
};
```

ICCM (new) node (used in L3 platforms):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			iccm: interrupt-controller@4020000 {
				compatible = "syntacore,iccm-mmio";
				reg = <0 0x4020000 0 0x20000>;
			};
		};
	};
};
```

L2 cache node (address and size for L2 cache region got from this node):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			scr_l2_cache: cache-controller@e8000000 {
				compatible = "syntacore,l2-cache";
				reg = <0 0xe8000000 0 0x2000>;
			};
		};
	};
};
```

L3 cache node (address and size for L3 cache region got from this node):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			scr_l3_cache: cache-controller@4004000 {
				compatible = "syntacore,l3-cache";
				reg = <0 0x4004000 0 0x4000>;
			};
		};
	};
};
```

L2 generic cache node (address and size for L2 generic cache region got from
this node):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			scr_l2_cache: cache-controller@e8001000 {
				compatible = "syntacore,l2-generic-cache";
				reg = <0 0xe8001000 0 0x1000>;
			};
		};
	};
};
```

L3 generic cache node (address and size for L3 generic cache region got from
this node):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			scr_l3_cache: cache-controller@4006000 {
				compatible = "syntacore,l3-generic-cache";
				reg = <0 0x4006000 0 0x2000>;
			};
		};
	};
};
```

Firmware node (contains hardware build ID)
```
/{
	firmware {
		syntacore,bldid = "0000000000000000";
	};
};
```

SCR FPGA node (contains addresses for BUILDID, SYSCLK and CLSCLK if availiable):
```
/{
	scr-fpga-reg {
		compatible = "syntacore,fpga-reg-v1";
		syntacore,buildid-reg = <ADDR_HLPART(0xc000000)>;
		syntacore,sysclk-reg = <ADDR_HLPART(0xc001000)>;
		syntacore,clsclk-reg = <ADDR_HLPART(0xc002000)>;
	};
};
```

Syntacore mtimer (can contain "scr,timsrc-external" option if clock source is
external):
```
/{
	soc {
		cluster_bus: cluster_bus {
			...
			mtimer: timer@4010000 {
				compatible = "syntacore,mtimer";
				reg = <0 0x4010000 0 0x2000>;
				scr,timsrc-external;
			};
		};
	};
};
```

Hardware info (here syntacore,struct-addr is an address where hardware
settings are placed):
```
/ {
	scr-hwinfo-struct {
		compatible = "syntacore,hwinfo-struct-v1";
		syntacore,struct-addr = <ADDR_HLPART(0x8000000000)>;
	};
};
```

Root domain region (allows to add one memory region to OpenSBI root domain
with permissions specified in 'flags' field):
```
/{
	cluster-mmio@10000000 {
		compatible = "syntacore,root-domain-region";
		reg = <ADDR_HLPART(0x10000000) ADDR_HLPART(0x20000)>;
		flags = <(SCR_PMP_MMODE_READ | SCR_PMP_MMODE_WRITE | SCR_MEMREGION_MMIO)>;
	};
};
```

Software pagewalker (enabled by default for SCR5 without dtb node):
```
/{
	cpus {
		cpu@0 {
			scr,software-pagewalker;
		};
	};
};
```

QEMU Specific Instructions
--------------------------
If you want to test OpenSBI with QEMU scrX machine, please follow the
instructions here.

First, download latest Syntacore toolchain with QEMU.
Second, download latest release OS pack for preferred platform.
Launch string will look like example below.

```
IMAGES=/path/to/os-pack
MBOOT=$IMAGES/miniboot/rv64_pmp_l2/miniboot.bin
OSBI=$IMAGES/fw_jump.bin
DTB=$IMAGES/dtbs/vcu118_scr7_l2.dtb
UBOOT=$IMAGES/u-boot-l2_cluster_opensbi-v2025.10.bin
LINUX=$IMAGES/linux/Image.bin
RAMDISK=$IMAGES/rootfs.cpio.gz

QEMU=/path/to/sc-devtoolkit/qemu/qemu-system-riscv64

$QEMU -m 4G -machine scr7_l2 -nographic \
	-M firmware=$MBOOT \
	-M sbi=$OSBI \
	-M uboot=$UBOOT \
	-M dtb=$DTB \
	-M kernel=$LINUX \
	-M initrd=$RAMDISK
```
