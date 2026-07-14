/*
 * K230 DWC SSI compatible SPI/QSPI controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_SSI_K230_DW_SSI_H
#define HW_SSI_K230_DW_SSI_H

#include "hw/core/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo32.h"
#include "qom/object.h"

#define TYPE_K230_DW_SSI "riscv.k230.dw-ssi"
OBJECT_DECLARE_SIMPLE_TYPE(K230DwSsiState, K230_DW_SSI)

#define K230_DW_SSI_MMIO_SIZE 0x1000
#define K230_DW_SSI_REGS_SIZE 0x14c
#define K230_DW_SSI_NUM_REGS \
    (K230_DW_SSI_REGS_SIZE / sizeof(uint32_t))

typedef struct K230DwSsiXip {
    MemoryRegion mmio;
    bool enabled;
    hwaddr window_size;
} K230DwSsiXip;

struct K230DwSsiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    K230DwSsiXip xip;
    SSIBus *spi;
    qemu_irq irq;
    qemu_irq *cs_lines;

    Fifo32 tx_fifo;
    Fifo32 rx_fifo;
    uint32_t regs[K230_DW_SSI_NUM_REGS];

    uint32_t num_cs;
    uint32_t max_lines;
    int active_cs;
};

#endif /* HW_SSI_K230_DW_SSI_H */
