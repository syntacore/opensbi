Syntacore Platform
==================

**Syntacore**

Build OpenSBI in jump to firmware mode, jump address is specified
in .config:

$ CROSS_COMPILE=riscv64-unknown-elf- make \
  PLATFORM=syntacore PLATFORM_DEFCONFIG=vcu118_scr7_defconfig

 => build/platform/syntacore/firmware/fw_jump.bin

Build OpenSBI with Linux as payload:

$ CROSS_COMPILE=riscv64-unknown-elf- make \
  PLATFORM=syntacore PLATFORM_DEFCONFIG=vcu118_scr7_defconfig \
  FW_PAYLOAD_PATH=</path/to/linux/arch/riscv/boot/Image>

  => build/platform/syntacore/firmware/fw_payload.bin

Build OpenSBI with U-Boot as payload:

$ CROSS_COMPILE=riscv64-unknown-elf- make \
  PLATFORM=syntacore PLATFORM_DEFCONFIG=vcu118_scr7_defconfig \
  FW_PAYLOAD_PATH=</path/to/u-boot/u-boot-dtb/bin>

  => build/platform/syntacore/firmware/fw_payload.bin

Build OpenSBI in fw_dynamic mode:

  to be done...

$ CROSS_COMPILE=riscv64-unknown-elf- make \
  PLATFORM=syntacore PLATFORM_DEFCONFIG=vcu118_scr7_defconfig

$ CROSS_COMPILE=riscv64-unknown-elf- make \
  make ...spl_defconfig \
$ export OPENSBI=</path/to/fw_dynamic.bin>
$ CROSS_COMPILE=riscv64-unknown-elf- make \
  make ... \
