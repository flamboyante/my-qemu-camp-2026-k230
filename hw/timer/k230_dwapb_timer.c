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

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "migration/vmstate.h"
#include "hw/core/sysbus.h"
#include "hw/core/ptimer.h"
#include "hw/core/qdev-clock.h"
#include "hw/timer/k230_dwapb_timer.h"
#include "trace.h"

static void k230_timer_clk_update(void *opaque, ClockEvent event)
{
    K230TimerState *s = K230_TIMER(opaque);
    for (int i = 0; i < K230_APBTMR_NUM_TIMERS; i++) {
        if (!s->timers[i].ptimer) {
            continue;
        }
        ptimer_transaction_begin(s->timers[i].ptimer);
        ptimer_set_period_from_clock(s->timers[i].ptimer, s->pclk, 1);
        ptimer_transaction_commit(s->timers[i].ptimer);
    }
}

static void k230_timer_enable(K230Timer *t)
{
    ptimer_transaction_begin(t->ptimer);
    ptimer_set_limit(t->ptimer, t->load ? t->load : 1, 1);
    ptimer_run(t->ptimer, 1);
    ptimer_transaction_commit(t->ptimer);

    trace_k230_timer_enable(t->id, t->load);
}

static void k230_timer_disable(K230Timer *t)
{
    ptimer_transaction_begin(t->ptimer);
    ptimer_stop(t->ptimer);
    ptimer_transaction_commit(t->ptimer);

    t->int_status = 0;
    qemu_set_irq(t->irq, 0);

    trace_k230_timer_disable(t->id);
}

static void k230_timer_tick(void *opaque)
{
    K230Timer *t = opaque;
    uint32_t reload;

    trace_k230_timer_tick(t->id);

    t->int_status = 1;

    if (!(t->control & K230_APBTMR_CONTROL_INT)) {
        qemu_set_irq(t->irq, 1);
        trace_k230_timer_interrupt(t->id);
    }

    if (t->control & K230_APBTMR_CONTROL_MODE_PERIODIC) {
        reload = t->load ? t->load : 1;
    } else {
        reload = UINT32_MAX;
    }

    ptimer_set_limit(t->ptimer, reload, 1);
    ptimer_run(t->ptimer, 1);
}

static uint64_t k230_timer_read(void *opaque, hwaddr addr, unsigned int size)
{
    K230TimerState *s = K230_TIMER(opaque);
    uint32_t value = 0;

    addr &= 0xfff;

    if (addr < K230_APBTMRS_INT_STATUS) {
        int idx = addr / K230_APBTMR_STRIDE;
        int reg = addr % K230_APBTMR_STRIDE;

        if (idx < K230_APBTMR_NUM_TIMERS) {
            switch (reg) {
            case K230_APBTMR_N_LOAD_COUNT:
                value = s->timers[idx].load;
                break;
            case K230_APBTMR_N_CURRENT_VALUE:
                if (s->timers[idx].control & K230_APBTMR_CONTROL_ENABLE) {
                    value = ptimer_get_count(s->timers[idx].ptimer);
                } else {
                    value = 0;
                }
                break;
            case K230_APBTMR_N_CONTROL:
                value = s->timers[idx].control;
                break;
            case K230_APBTMR_N_EOI:
                s->timers[idx].int_status = 0;
                qemu_set_irq(s->timers[idx].irq, 0);
                trace_k230_timer_irq_clear(s->timers[idx].id);
                value = 0;
                break;
            case K230_APBTMR_N_INT_STATUS:
                if (s->timers[idx].control & K230_APBTMR_CONTROL_INT) {
                    value = 0;
                } else {
                    value = s->timers[idx].int_status;
                }
                break;
            }
        }
    } else if (addr >= K230_APBTMR_N_LOAD2 &&
               addr < K230_APBTMR_N_LOAD2 +
                      K230_APBTMR_NUM_TIMERS * K230_APBTMR_LOAD2_STRIDE) {
        int idx = (addr - K230_APBTMR_N_LOAD2) / K230_APBTMR_LOAD2_STRIDE;
        value = s->timers[idx].load2;
    } else {
        switch (addr) {
        case K230_APBTMRS_INT_STATUS:
            for (int i = 0; i < K230_APBTMR_NUM_TIMERS; i++) {
                if (!(s->timers[i].control & K230_APBTMR_CONTROL_INT)) {
                    value |= s->timers[i].int_status << i;
                }
            }
            break;
        case K230_APBTMRS_EOI:
            for (int i = 0; i < K230_APBTMR_NUM_TIMERS; i++) {
                s->timers[i].int_status = 0;
                qemu_set_irq(s->timers[i].irq, 0);
            }
            value = 0;
            break;
        case K230_APBTMRS_RAW_INT_STATUS:
            for (int i = 0; i < K230_APBTMR_NUM_TIMERS; i++) {
                value |= s->timers[i].int_status << i;
            }
            break;
        case K230_APBTMRS_COMP_VERSION:
            value = K230_APBTMR_COMP_VERSION_VAL;
            break;
        }
    }

    trace_k230_timer_read(addr, value);
    return value;
}

static void k230_timer_write(void *opaque, hwaddr addr,
                             uint64_t value, unsigned int size)
{
    K230TimerState *s = K230_TIMER(opaque);

    addr &= 0xfff;

    if (addr < K230_APBTMRS_INT_STATUS) {
        int idx = addr / K230_APBTMR_STRIDE;
        int reg = addr % K230_APBTMR_STRIDE;

        if (idx < K230_APBTMR_NUM_TIMERS) {
            switch (reg) {
            case K230_APBTMR_N_LOAD_COUNT:
                s->timers[idx].load = value;
                break;
            case K230_APBTMR_N_CONTROL: {
                uint32_t old_ctrl = s->timers[idx].control;
                s->timers[idx].control = value & K230_APBTMR_CONTROL_RW_MASK;
                if ((value ^ old_ctrl) & K230_APBTMR_CONTROL_ENABLE) {
                    if (value & K230_APBTMR_CONTROL_ENABLE) {
                        k230_timer_enable(&s->timers[idx]);
                    } else {
                        k230_timer_disable(&s->timers[idx]);
                    }
                }
                if ((value ^ old_ctrl) & K230_APBTMR_CONTROL_INT) {
                    if (value & K230_APBTMR_CONTROL_INT) {
                        qemu_set_irq(s->timers[idx].irq, 0);
                    } else if (s->timers[idx].int_status) {
                        qemu_set_irq(s->timers[idx].irq, 1);
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    } else if (addr >= K230_APBTMR_N_LOAD2 &&
               addr < K230_APBTMR_N_LOAD2 +
                      K230_APBTMR_NUM_TIMERS * K230_APBTMR_LOAD2_STRIDE) {
        int idx = (addr - K230_APBTMR_N_LOAD2) / K230_APBTMR_LOAD2_STRIDE;
        s->timers[idx].load2 = value;
    }

    trace_k230_timer_write(addr, value);
}

static const MemoryRegionOps k230_timer_ops = {
    .read  = k230_timer_read,
    .write = k230_timer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void k230_timer_realize(DeviceState *dev, Error **errp)
{
    K230TimerState *s = K230_TIMER(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    if (!clock_has_source(s->pclk)) {
        error_setg(errp, "K230 DW APB timer: pclk clock must be connected");
        return;
    }

    for (int i = 0; i < K230_APBTMR_NUM_TIMERS; i++) {
        s->timers[i].id = i;
        s->timers[i].ptimer = ptimer_init(k230_timer_tick, &s->timers[i],
            PTIMER_POLICY_NO_IMMEDIATE_TRIGGER |
            PTIMER_POLICY_NO_IMMEDIATE_RELOAD |
            PTIMER_POLICY_NO_COUNTER_ROUND_DOWN);
        ptimer_transaction_begin(s->timers[i].ptimer);
        ptimer_set_limit(s->timers[i].ptimer, UINT32_MAX, 1);
        ptimer_transaction_commit(s->timers[i].ptimer);
        sysbus_init_irq(sbd, &s->timers[i].irq);
    }

    k230_timer_clk_update(s, ClockUpdate);

    memory_region_init_io(&s->mmio, OBJECT(dev), &k230_timer_ops,
                          s, TYPE_K230_TIMER, K230_APBTMR_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);
}

static void k230_timer_reset(DeviceState *dev)
{
    K230TimerState *s = K230_TIMER(dev);

    for (int i = 0; i < K230_APBTMR_NUM_TIMERS; i++) {
        K230Timer *t = &s->timers[i];

        ptimer_transaction_begin(t->ptimer);
        ptimer_stop(t->ptimer);
        ptimer_transaction_commit(t->ptimer);

        t->load = 0;
        t->load2 = 0;
        t->control = 0;
        t->int_status = 0;
        qemu_set_irq(t->irq, 0);
    }
}

static const VMStateDescription vmstate_k230_timer_channel = {
    .name = "k230-dwapb-timer-channel",
    .fields = (const VMStateField[]) {
        VMSTATE_PTIMER(ptimer, K230Timer),
        VMSTATE_UINT32(load, K230Timer),
        VMSTATE_UINT32(load2, K230Timer),
        VMSTATE_UINT32(control, K230Timer),
        VMSTATE_UINT32(int_status, K230Timer),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_k230_timer = {
    .name = "k230-dwapb-timer",
    .fields = (const VMStateField[]) {
        VMSTATE_CLOCK(pclk, K230TimerState),
        VMSTATE_STRUCT_ARRAY(timers, K230TimerState,
                             K230_APBTMR_NUM_TIMERS, 0,
                             vmstate_k230_timer_channel, K230Timer),
        VMSTATE_END_OF_LIST()
    }
};

static void k230_timer_init(Object *obj)
{
    K230TimerState *s = K230_TIMER(obj);
    s->pclk = qdev_init_clock_in(DEVICE(obj), "pclk", k230_timer_clk_update,
                                 s, ClockUpdate);
}

static void k230_timer_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = k230_timer_realize;
    dc->vmsd = &vmstate_k230_timer;
    dc->desc = "K230 DW APB timer";
    device_class_set_legacy_reset(dc, k230_timer_reset);
}

static const TypeInfo k230_timer_info = {
    .name          = TYPE_K230_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(K230TimerState),
    .instance_init = k230_timer_init,
    .class_init    = k230_timer_class_init,
};

static void k230_timer_register_type(void)
{
    type_register_static(&k230_timer_info);
}
type_init(k230_timer_register_type)
