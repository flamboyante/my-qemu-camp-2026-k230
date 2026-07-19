/*
 * K230 Reset Management Unit (RMU / SYSCTL_RST)
 *
 * K230 Technical Reference Manual V0.3.1 (2024-11-18):
 * https://github.com/revyos/external-docs/blob/master/K230/en-us/K230_Technical_Reference_Manual_V0.3.1_20241118.pdf
 *
 * Register semantics cross-checked against the Linux mainline driver
 * drivers/reset/reset-k230.c (compatible "canaan,k230-rst").
 *
 * Copyright (c) 2026 Jack Wang <163wangjack@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_K230_RMU_H
#define HW_MISC_K230_RMU_H

#include "qemu/bitops.h"
#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_K230_RMU "riscv.k230.rmu"
OBJECT_DECLARE_SIMPLE_TYPE(K230RmuState, K230_RMU)

/* 1 KiB MMIO window, see K230_DEV_RMU in hw/riscv/k230.c. */
#define K230_RMU_MMIO_SIZE   0x1000
#define K230_RMU_NUM_REGS    (K230_RMU_MMIO_SIZE / 4)

/*
 * Control-register offsets used by drivers/reset/reset-k230.c. The driver names
 * differ from the K230 TRM (chapter 2.1 "Reset"), but the offsets and bit
 * layouts match the TRM's "*_RST_CTL" registers, named in the trailing comment.
 * The documented reset values live in k230_rmu_regs[] in k230_rmu.c.
 */
#define K230_RMU_CPU0_CTRL     0x04   /* CPU0_RST_CTL */
#define K230_RMU_CPU1_CTRL     0x0C   /* CPU1_RST_CTL */
#define K230_RMU_AI_CTRL       0x14   /* AI_RST_CTL */
#define K230_RMU_VPU_CTRL      0x1C   /* VPU_RST_CTL */
#define K230_RMU_PERI0_CTRL    0x20   /* SOC_CTL_RST_CTL */
/*
 * PERI0 (SOC_CTL_RST_CTL) software-reset bits for the two watchdogs. In the TRM
 * these are wdt_0_reset (bit12) / wdt_1_reset (bit13), whose reset value is 1
 * ("reset disassert"). The model treats a write of these bits as "trigger a
 * reset", driving a real cold reset of the linked WDT devices (see the
 * "wdt0"/"wdt1" QOM links).
 */
#define K230_RMU_PERI0_WDT0_RST  BIT(12)
#define K230_RMU_PERI0_WDT1_RST  BIT(13)
#define K230_RMU_PERI1_CTRL    0x24   /* LOSYS_RST_CTL */
#define K230_RMU_HISYS_CTRL    0x2C   /* HISYS_RST_CTL */
#define K230_RMU_SDIO_CTRL     0x34   /* SDC_RST_CTL */
#define K230_RMU_USB_CTRL      0x3C   /* USB_RST_CTL */
#define K230_RMU_SPI_CTRL      0x44   /* SPI_RST_CTL */
#define K230_RMU_SEC_CTRL      0x4C   /* SEC_RST_CTL */
#define K230_RMU_DMA_CTRL      0x54   /* DMA_RST_CTL */
#define K230_RMU_DECOMP_CTRL   0x5C   /* DECOMPRESS_RST_CTL */
#define K230_RMU_SRAM_CTRL     0x64   /* SRAM_RST_CTL */
#define K230_RMU_NONAI2D_CTRL  0x6C   /* NONAI2D_RST_CTL */
#define K230_RMU_MCTL_CTRL     0x74   /* MCTL_RST_CTL */
#define K230_RMU_ISP_CTRL      0x80   /* ISP_RST_CTL */
#define K230_RMU_DPU_CTRL      0x88   /* DPU_RST_CTL */
#define K230_RMU_DISP_CTRL     0x90   /* DISP_RST_CTL */
#define K230_RMU_GPU_CTRL      0x98   /* V2P5D_RST_CTL */
#define K230_RMU_AUDIO_CTRL    0xA4   /* AUDIO_RST_CTL */
#define K230_RMU_SPI2AXI_CTRL  0xA8   /* SW_DONE (not in TRM) */

/*
 * Reset-time-control ("*_RST_TIM") registers. These interleave with the control
 * registers above and are plain read/write timing storage with no side effects,
 * so the model backs them with their documented reset value (see k230_rmu.c).
 */
#define K230_RMU_CPU0_TIM      0x00   /* CPU0_RST_TIM */
#define K230_RMU_CPU1_TIM      0x08   /* CPU1_RST_TIM */
#define K230_RMU_AI_TIM        0x10   /* AI_RST_TIM */
#define K230_RMU_VPU_TIM       0x18   /* VPU_RST_TIM */
#define K230_RMU_HISYS_TIM     0x28   /* HISYS_HCLK_TIM */
#define K230_RMU_SDCTL_TIM     0x30   /* SDCTL_RST_TIM */
#define K230_RMU_USB_TIM       0x38   /* USB_RST_TIM */
#define K230_RMU_SPI_TIM       0x40   /* SPI_RST_TIM */
#define K230_RMU_SEC_TIM       0x48   /* SEC_SYS_RST_TIM */
#define K230_RMU_DMAC_TIM      0x50   /* DMAC_RST_TIM */
#define K230_RMU_DECOMP_TIM    0x58   /* DECOMPRESS_RST_TIM */
#define K230_RMU_SRAM_TIM      0x60   /* SRAM_RST_TIM */
#define K230_RMU_NONAI2D_TIM   0x68   /* NONAI2D_RST_TIM */
#define K230_RMU_MCTL_TIM      0x70   /* MCTL_RST_TIM */
#define K230_RMU_ISP_TIM       0x78   /* ISP_RST_TIM */
#define K230_RMU_ISP_DW_TIM    0x7C   /* ISP_DW_RST_TIM */
#define K230_RMU_DPU_TIM       0x84   /* DPU_RST_TIM */
#define K230_RMU_DISP_TIM      0x8C   /* DISP_SYS_RST_TIM */
#define K230_RMU_V2P5D_TIM     0x94   /* V2P5D_SYS_RST_TIM */
#define K230_RMU_AUDIO_TIM     0xA0   /* AUDIO_RST_TIM */

/* Bit layout shared by the CPU0/CPU1 control registers. */
#define K230_RMU_CPU_RESET     BIT(0)    /* reset request, auto/soft cleared */
#define K230_RMU_CPU_FLUSH     BIT(4)    /* L2 flush, hw auto-clears */
#define K230_RMU_CPU_DONE      BIT(12)   /* done bit, write-1-to-clear */

/* The high 16 bits are per-bit write-enable strobes (CPU0/CPU1 registers). */
#define K230_RMU_WE_SHIFT      16

/* Devices the RMU can cold-reset: WDT0, WDT1 (via the "wdt0"/"wdt1" links). */
#define K230_RMU_NUM_TARGETS   2

struct K230RmuState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;

    /*
     * One 32-bit word per MMIO offset. Indexing by word offset keeps the
     * VMState description trivial. Reset requests auto-clear, so what
     * actually persists here is done bits and SW_DONE storage bits.
     */
    uint32_t regs[K230_RMU_NUM_REGS];

    /*
     * Optional links to the peripherals the RMU actually resets. Populated by
     * the machine via the "wdt0"/"wdt1" QOM link properties; a NULL entry just
     * means "no device connected" and the reset request is a no-op.
     */
    DeviceState *reset_targets[K230_RMU_NUM_TARGETS];
};

#endif /* HW_MISC_K230_RMU_H */
