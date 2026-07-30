/*
 * K230 DW APB timer
 *
 * Copyright (c) 2026 raoyi <rao232328@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * K230 Technical Reference Manual V0.3.1 (2024-11-18):
 * https://github.com/revyos/external-docs/blob/master/K230/en-us/K230_Technical_Reference_Manual_V0.3.1_20241118.pdf
 */

#ifndef K230_DWAPB_TIMER_H
#define K230_DWAPB_TIMER_H

#include "qemu/bitops.h"
#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "hw/core/clock.h"
#include "qom/object.h"

#define TYPE_K230_TIMER "riscv.k230.timer"
OBJECT_DECLARE_SIMPLE_TYPE(K230TimerState, K230_TIMER)

#define K230_APBTMR_MMIO_SIZE    0x800
#define K230_APBTMR_NUM_TIMERS   6
#define K230_APBTMR_STRIDE       0x14
#define K230_APBTMR_DEFAULT_FREQ 6250000
#define K230_APBTMR_COMP_VERSION_VAL 0x3231312A
#define K230_APBTMR_LOAD2_STRIDE  0x04

/* Per-timer register offsets */
#define K230_APBTMR_N_LOAD_COUNT    0x00
#define K230_APBTMR_N_CURRENT_VALUE 0x04
#define K230_APBTMR_N_CONTROL       0x08
#define K230_APBTMR_N_EOI           0x0c
#define K230_APBTMR_N_INT_STATUS    0x10
/* Global register offsets */
#define K230_APBTMRS_INT_STATUS        0xa0
#define K230_APBTMRS_EOI               0xa4
#define K230_APBTMRS_RAW_INT_STATUS    0xa8
#define K230_APBTMRS_COMP_VERSION      0xac
/* PWM LoadCount2 registers */
#define K230_APBTMR_N_LOAD2       0xb0

/* Control register bits */
#define K230_APBTMR_CONTROL_ENABLE        BIT(0)
#define K230_APBTMR_CONTROL_MODE_PERIODIC BIT(1)
#define K230_APBTMR_CONTROL_INT           BIT(2)
#define K230_APBTMR_CONTROL_PWM           BIT(3)
#define K230_APBTMR_CONTROL_RW_MASK     0xf

typedef struct K230Timer {
    struct ptimer_state *ptimer;
    qemu_irq irq;
    int id;
    uint32_t load;
    uint32_t load2;
    uint32_t control;
    uint32_t int_status;
} K230Timer;

struct K230TimerState {
    /* <private> */
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    K230Timer timers[K230_APBTMR_NUM_TIMERS];
    Clock *pclk;
};

#endif
