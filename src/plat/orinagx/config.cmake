#
# Copyright 2024, Unikie
#
# SPDX-License-Identifier: GPL-2.0-only
#

cmake_minimum_required(VERSION 3.7.2)

declare_platform(orinagx KernelPlatformOrinAGX PLAT_ORIN_AGX KernelSel4ArchAarch64)

if(KernelPlatformOrinAGX)
    declare_seL4_arch(aarch64)

    set(KernelArmCortexA78 ON)
    set(KernelArchArmV8a ON)
    # Note: PA size is set to 40 bits in arch/arm/config.cmake for Orin
    # to match Tegra234 memory controller hardware limits.
    set(KernelArmGicV3 ON)
    set(KernelArmSMMU OFF)
    # Disabled to debug RAS errors - will halt with debug info on SError
    set(KernelAArch64SErrorIgnore OFF)
    # Enable user-space access to generic timer for ltimer
    set(KernelArmExportPCNTUser ON)
    set(KernelArmExportPTMRUser ON)

    config_set(KernelARMPlatform ARM_PLAT orinagx)
    # Note: We don't set KernelArmMach because Orin uses SBSA UART
    # (not the legacy nvidia UARTs from TX1/TX2)

    list(APPEND KernelDTSList "tools/dts/orinagx.dts")
    list(APPEND KernelDTSList "src/plat/orinagx/overlay-orinagx.dts")

    declare_default_headers(
        TIMER_FREQUENCY 31250000
        MAX_IRQ 512
        INTERRUPT_CONTROLLER arch/machine/gic_v3.h
        NUM_PPI 32
        TIMER drivers/timer/arm_generic.h
    )
endif()

add_sources(
    DEP "KernelPlatformOrinAGX"
    CFILES src/arch/arm/machine/l2c_nop.c src/arch/arm/machine/gic_v3.c
)

