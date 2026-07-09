/*
 * K230 DWC SSI compatible SPI/QSPI controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_SSI_K230_DW_SSI_H
#define HW_SSI_K230_DW_SSI_H

#include "hw/core/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo8.h"
#include "qom/object.h"

#define TYPE_K230_DW_SSI "riscv.k230.dw-ssi"
OBJECT_DECLARE_SIMPLE_TYPE(K230DwSsiState, K230_DW_SSI)

#define K230_DW_SSI_MMIO_SIZE 0x1000

enum K230DwSsiRegister {
    K230_DW_SSI_CTRLR0          = 0x000,
    K230_DW_SSI_CTRLR1          = 0x004,
    K230_DW_SSI_SSIENR          = 0x008,
    K230_DW_SSI_MWCR            = 0x00c,
    K230_DW_SSI_SER             = 0x010,
    K230_DW_SSI_BAUDR           = 0x014,
    K230_DW_SSI_TXFTLR          = 0x018,
    K230_DW_SSI_RXFTLR          = 0x01c,
    K230_DW_SSI_TXFLR           = 0x020,
    K230_DW_SSI_RXFLR           = 0x024,
    K230_DW_SSI_SR              = 0x028,
    K230_DW_SSI_IMR             = 0x02c,
    K230_DW_SSI_ISR             = 0x030,
    K230_DW_SSI_RISR            = 0x034,
    K230_DW_SSI_TXEICR          = 0x038,
    K230_DW_SSI_RXOICR          = 0x03c,
    K230_DW_SSI_RXUICR          = 0x040,
    K230_DW_SSI_MSTICR          = 0x044,
    K230_DW_SSI_ICR             = 0x048,
    K230_DW_SSI_DMACR           = 0x04c,
    K230_DW_SSI_DMATDLR         = 0x050,
    K230_DW_SSI_DMARDLR         = 0x054,
    K230_DW_SSI_IDR             = 0x058,
    K230_DW_SSI_SSIC_VERSION_ID = 0x05c,
    K230_DW_SSI_DR0             = 0x060,
    K230_DW_SSI_DR_END          = 0x0ec,
    K230_DW_SSI_RX_SAMPLE_DELAY = 0x0f0,
    K230_DW_SSI_SPI_CTRLR0      = 0x0f4,
    K230_DW_SSI_TXD_DRIVE_EDGE  = 0x0f8,
    K230_DW_SSI_XIP_MODE_BITS   = 0x0fc,
    K230_DW_SSI_XIP_INCR_INST   = 0x100,
    K230_DW_SSI_XIP_WRAP_INST   = 0x104,
    K230_DW_SSI_XIP_CTRL        = 0x108,
    K230_DW_SSI_XIP_SER         = 0x10c,
    K230_DW_SSI_XRXOICR         = 0x110,
    K230_DW_SSI_XIP_CNT_TIME_OUT = 0x114,
    K230_DW_SSI_SPI_CTRLR1      = 0x118,
};

struct K230DwSsiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    MemoryRegion xip;
    SSIBus *spi;
    qemu_irq irq;
    qemu_irq *cs_lines;

    Fifo8 tx_fifo;
    Fifo8 rx_fifo;
    uint32_t regs[K230_DW_SSI_MMIO_SIZE / sizeof(uint32_t)];

    uint32_t num_cs;
    uint32_t max_lines;
    bool has_xip;
    hwaddr flash_window_size;
    int active_cs;
};

#endif /* HW_SSI_K230_DW_SSI_H */
