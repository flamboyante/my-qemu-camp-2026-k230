/*
 * Synopsys DesignWare SSI
 *
 * Copyright (c) 2026 Kangjie Huang <flamboyant.h.01@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Emulates a Synopsys DesignWare SSI SPI controller.
 *
 */

#ifndef HW_SSI_DW_SSI_H
#define HW_SSI_DW_SSI_H

#include "hw/core/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo32.h"
#include "qom/object.h"

#define TYPE_DW_SSI "designware-ssi"
OBJECT_DECLARE_SIMPLE_TYPE(DwSsiState, DW_SSI)

#define DW_SSI_MMIO_SIZE 0x1000
#define DW_SSI_REGS_SIZE 0x14c
#define DW_SSI_NUM_REGS \
    (DW_SSI_REGS_SIZE / sizeof(uint32_t))

/*
 * DwSsiConfig: per-instance configuration properties.
 *
 * These are set by the SoC/machine code before realize and are
 * immutable for the lifetime of the device.
 */
typedef struct DwSsiConfig {
    uint32_t num_cs;
    uint32_t fifo_depth;
    uint32_t imr_reset;
} DwSsiConfig;

/* SSI GPIO output ordering differs from RISR/ISR bit ordering. */
typedef enum DwSsiIrq {
    DW_SSI_IRQ_TXE,
    DW_SSI_IRQ_TXO,
    DW_SSI_IRQ_RXF,
    DW_SSI_IRQ_RXO,
    DW_SSI_IRQ_TXU,
    DW_SSI_IRQ_RXU,
    DW_SSI_IRQ_MST,
    DW_SSI_IRQ_COUNT,
} DwSsiIrq;

typedef enum DwSsiPhase {
    DW_SSI_PHASE_IDLE,
    DW_SSI_PHASE_STANDARD_TX_ONLY,
    DW_SSI_PHASE_RX_ONLY,
    DW_SSI_PHASE_EEPROM_COMMAND,
    DW_SSI_PHASE_EEPROM_DATA,

} DwSsiPhase;

struct DwSsiState {
    SysBusDevice parent_obj;

    DwSsiConfig cfg;

    MemoryRegion mmio;
    SSIBus *spi;

    qemu_irq *cs_lines;
    qemu_irq irqs[DW_SSI_IRQ_COUNT];

    Fifo32 tx_fifo;
    Fifo32 rx_fifo;
    uint32_t regs[DW_SSI_NUM_REGS];

    uint32_t irq_latched;

    uint32_t phase;
    uint32_t remaining_frames;
    uint32_t max_lines;
    int active_cs;
};


#endif /* HW_SSI_DW_SSI_H */
