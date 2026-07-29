/*
 * Synopsys DesignWare SSI
 *
 * Copyright (c) 2026 Kangjie Huang <flamboyant.h.01@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Emulates the DesignWare SSI controllers, including standard SPI,
 * Dual/Quad SDR, internal DMA, and the XIP read window.
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
#define DW_SSI_XIP_WINDOW_SIZE 0x08000000

/* SSI GPIO output ordering differs from RISR/ISR bit ordering. */
typedef enum DwSsiIrq {
    DW_SSI_IRQ_TXE,
    DW_SSI_IRQ_TXO,
    DW_SSI_IRQ_RXF,
    DW_SSI_IRQ_RXO,
    DW_SSI_IRQ_TXU,
    DW_SSI_IRQ_RXU,
    DW_SSI_IRQ_MST,
    DW_SSI_IRQ_DONE,
    DW_SSI_IRQ_AXIE,
    DW_SSI_IRQ_COUNT,
} DwSsiIrq;

typedef enum DwSsiPhase {
    DW_SSI_PHASE_IDLE,
    DW_SSI_PHASE_STANDARD_TX_ONLY,
    DW_SSI_PHASE_RX_ONLY,
    DW_SSI_PHASE_EEPROM_COMMAND,
    DW_SSI_PHASE_EEPROM_DATA,

    DW_SSI_PHASE_ENHANCED_INSTRUCTION,
    DW_SSI_PHASE_ENHANCED_ADDRESS,
    DW_SSI_PHASE_ENHANCED_DUMMY,
    DW_SSI_PHASE_ENHANCED_DATA,
} DwSsiPhase;

typedef struct DwSsiEnhancedCommand {
    uint32_t instruction;
    uint32_t address;
    uint32_t instruction_bits;
    uint32_t address_bits;
    uint32_t wait_cycles;
    uint32_t data_frames;
    uint32_t spi_frf;
    uint32_t trans_type;
    uint32_t tmod;

    /* XIP-only fields; ordinary enhanced transfers do not consume them. */
    uint32_t mode;
    uint32_t mode_bits;
    bool mode_bits_enabled;
} DwSsiEnhancedCommand;

struct DwSsiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    MemoryRegion xip;
    SSIBus *spi;

    qemu_irq *cs_lines;
    qemu_irq irqs[DW_SSI_IRQ_COUNT];

    Fifo32 tx_fifo;
    Fifo32 rx_fifo;
    uint32_t regs[DW_SSI_NUM_REGS];

    uint32_t irq_latched;
    uint32_t idma_completed_frames;

    uint32_t phase;
    uint32_t remaining_frames;
    DwSsiEnhancedCommand enhanced;

    uint32_t num_cs;
    uint32_t max_lines;
    int active_cs;
    bool sleep_status;
    bool xip_enabled;
};

uint32_t dw_ssi_get_spi_mode(const DwSsiState *s);
bool dw_ssi_is_sleeping(const DwSsiState *s);

#endif /* HW_SSI_DW_SSI_H */
