/*
 * K230 DWC SSI compatible SPI/QSPI controller
 *
 * This model implements the register/FIFO/SSI paths needed by K230
 * controller-level tests and a read-only SPI NOR XIP window.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/registerfields.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/irq.h"
#include "hw/ssi/k230_dw_ssi.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define K230_DW_SSI_FIFO_CAPACITY 32

#define K230_DW_SSI_CTRLR0_RESET 0x00004007
#define K230_DW_SSI_VERSION      0x3130332a
#define K230_DW_SSI_DEFAULT_READ 0x03

REG32(CTRLR0, 0x000)
REG32(CTRLR1, 0x004)
REG32(SSIENR, 0x008)
    FIELD(SSIENR, SSIC_EN, 0, 1)
REG32(MWCR, 0x00c)
REG32(SER, 0x010)
REG32(BAUDR, 0x014)
REG32(TXFTLR, 0x018)
    FIELD(TXFTLR, TFT, 0, 5)
REG32(RXFTLR, 0x01c)
    FIELD(RXFTLR, RFT, 0, 5)
REG32(TXFLR, 0x020)
REG32(RXFLR, 0x024)
REG32(SR, 0x028)
    FIELD(SR, BUSY, 0, 1)
    FIELD(SR, TFNF, 1, 1)
    FIELD(SR, TFE, 2, 1)
    FIELD(SR, RFNE, 3, 1)
    FIELD(SR, RFF, 4, 1)
REG32(IMR, 0x02c)
    FIELD(IMR, TXEIM, 0, 1)
    FIELD(IMR, RXFIM, 4, 1)
REG32(ISR, 0x030)
    FIELD(ISR, TXEIS, 0, 1)
    FIELD(ISR, RXFIS, 4, 1)
REG32(RISR, 0x034)
    FIELD(RISR, TXEIR, 0, 1)
    FIELD(RISR, RXFIR, 4, 1)
REG32(TXEICR, 0x038)
REG32(RXOICR, 0x03c)
REG32(RXUICR, 0x040)
REG32(MSTICR, 0x044)
REG32(ICR, 0x048)
REG32(DMACR, 0x04c)
REG32(DMATDLR, 0x050)
REG32(DMARDLR, 0x054)
REG32(IDR, 0x058)
REG32(SSIC_VERSION_ID, 0x05c)
REG32(DR0, 0x060)
REG32(DR_END, 0x0ec)
REG32(RX_SAMPLE_DELAY, 0x0f0)
REG32(SPI_CTRLR0, 0x0f4)
REG32(DDR_DRIVE_EDGE, 0x0f8)
REG32(XIP_MODE_BITS, 0x0fc)
REG32(XIP_INCR_INST, 0x100)
    FIELD(XIP_INCR_INST, INCR_INST, 0, 16)
REG32(XIP_WRAP_INST, 0x104)
REG32(XIP_CTRL, 0x108)
REG32(XIP_SER, 0x10c)
REG32(XRXOICR, 0x110)
REG32(XIP_CNT_TIME_OUT, 0x114)
REG32(SPI_CTRLR1, 0x118)

static bool k230_dw_ssi_enabled(K230DwSsiState *s)
{
    return FIELD_EX32(s->regs[R_SSIENR], SSIENR, SSIC_EN);
}

static void k230_dw_ssi_deselect(K230DwSsiState *s)
{
    if (s->active_cs < 0) {
        return;
    }

    qemu_irq_raise(s->cs_lines[s->active_cs]);
    s->active_cs = -1;
}

static void k230_dw_ssi_select(K230DwSsiState *s, unsigned cs)
{
    if (cs >= s->num_cs) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid chip select %u\n",
                      DEVICE(s)->canonical_path, cs);
        k230_dw_ssi_deselect(s);
        return;
    }

    if (s->active_cs == cs) {
        return;
    }

    k230_dw_ssi_deselect(s);
    qemu_irq_lower(s->cs_lines[cs]);
    s->active_cs = cs;
}

static void k230_dw_ssi_update_cs(K230DwSsiState *s)
{
    uint32_t ser = s->regs[R_SER];

    if (!k230_dw_ssi_enabled(s) || !ser) {
        k230_dw_ssi_deselect(s);
        return;
    }

    k230_dw_ssi_select(s, ctz32(ser));
}

static uint32_t k230_dw_ssi_raw_irq(K230DwSsiState *s)
{
    uint32_t raw = 0;
    uint32_t tx_threshold = FIELD_EX32(s->regs[R_TXFTLR], TXFTLR, TFT);
    uint32_t rx_threshold = FIELD_EX32(s->regs[R_RXFTLR], RXFTLR, RFT);

    if (fifo8_num_used(&s->tx_fifo) <= tx_threshold) {
        raw = FIELD_DP32(raw, RISR, TXEIR, 1);
    }

    if (fifo8_num_used(&s->rx_fifo) > rx_threshold) {
        raw = FIELD_DP32(raw, RISR, RXFIR, 1);
    }

    return raw;
}

static uint32_t k230_dw_ssi_irq_status(K230DwSsiState *s)
{
    uint32_t raw = k230_dw_ssi_raw_irq(s);
    uint32_t mask = s->regs[R_IMR];
    uint32_t status = 0;

    status = FIELD_DP32(status, ISR, TXEIS,
                        FIELD_EX32(raw, RISR, TXEIR) &&
                        FIELD_EX32(mask, IMR, TXEIM));
    status = FIELD_DP32(status, ISR, RXFIS,
                        FIELD_EX32(raw, RISR, RXFIR) &&
                        FIELD_EX32(mask, IMR, RXFIM));

    return status;
}

static void k230_dw_ssi_update_irq(K230DwSsiState *s)
{
    qemu_set_irq(s->irq, !!k230_dw_ssi_irq_status(s));
}

static uint32_t k230_dw_ssi_status(K230DwSsiState *s)
{
    uint32_t tx_used = fifo8_num_used(&s->tx_fifo);
    uint32_t rx_used = fifo8_num_used(&s->rx_fifo);
    uint32_t sr = 0;

    sr = FIELD_DP32(sr, SR, TFNF, tx_used < K230_DW_SSI_FIFO_CAPACITY);
    sr = FIELD_DP32(sr, SR, TFE, tx_used == 0);
    sr = FIELD_DP32(sr, SR, RFNE, rx_used != 0);
    sr = FIELD_DP32(sr, SR, RFF,
                    rx_used == K230_DW_SSI_FIFO_CAPACITY);

    return sr;
}

static void k230_dw_ssi_transfer_byte(K230DwSsiState *s, uint8_t tx)
{
    uint8_t rx;

    if (!k230_dw_ssi_enabled(s) || s->active_cs < 0) {
        k230_dw_ssi_update_irq(s);
        return;
    }

    rx = ssi_transfer(s->spi, tx);
    if (!fifo8_is_full(&s->rx_fifo)) {
        fifo8_push(&s->rx_fifo, rx);
    }

    k230_dw_ssi_update_irq(s);
}

static bool k230_dw_ssi_is_dr(hwaddr addr)
{
    return addr >= A_DR0 && addr <= A_DR_END &&
           (addr & 0x3) == 0;
}

static uint64_t k230_dw_ssi_read(void *opaque, hwaddr addr, unsigned int size)
{
    K230DwSsiState *s = K230_DW_SSI(opaque);
    uint32_t value = 0;

    if (k230_dw_ssi_is_dr(addr)) {
        if (!fifo8_is_empty(&s->rx_fifo)) {
            value = fifo8_pop(&s->rx_fifo);
        }
        k230_dw_ssi_update_irq(s);
        return value;
    }

    switch (addr) {
    case A_TXFLR:
        value = fifo8_num_used(&s->tx_fifo);
        break;
    case A_RXFLR:
        value = fifo8_num_used(&s->rx_fifo);
        break;
    case A_SR:
        value = k230_dw_ssi_status(s);
        break;
    case A_RISR:
        value = k230_dw_ssi_raw_irq(s);
        break;
    case A_ISR:
        value = k230_dw_ssi_irq_status(s);
        break;
    case A_TXEICR:
    case A_RXOICR:
    case A_RXUICR:
    case A_MSTICR:
    case A_ICR:
    case A_XRXOICR:
        value = 0;
        break;
    case A_SSIC_VERSION_ID:
        value = s->regs[R_SSIC_VERSION_ID];
        break;
    case A_IDR:
        value = 0;
        break;
    default:
        if (addr < K230_DW_SSI_REGS_SIZE && (addr & 0x3) == 0) {
            value = s->regs[addr / sizeof(uint32_t)];
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: bad read offset 0x%" HWADDR_PRIx "\n",
                          DEVICE(s)->canonical_path, addr);
        }
        break;
    }

    return value;
}

static void k230_dw_ssi_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    K230DwSsiState *s = K230_DW_SSI(opaque);

    if (k230_dw_ssi_is_dr(addr)) {
        k230_dw_ssi_transfer_byte(s, value & 0xff);
        return;
    }

    switch (addr) {
    case A_CTRLR0:
    case A_CTRLR1:
    case A_MWCR:
    case A_BAUDR:
    case A_IMR:
    case A_DMACR:
    case A_DMATDLR:
    case A_DMARDLR:
    case A_RX_SAMPLE_DELAY:
    case A_SPI_CTRLR0:
    case A_DDR_DRIVE_EDGE:
    case A_XIP_MODE_BITS:
    case A_XIP_WRAP_INST:
    case A_XIP_CTRL:
    case A_XIP_SER:
    case A_XIP_CNT_TIME_OUT:
    case A_SPI_CTRLR1:
        s->regs[addr / sizeof(uint32_t)] = value;
        k230_dw_ssi_update_irq(s);
        break;
    case A_TXFTLR:
        s->regs[R_TXFTLR] = FIELD_DP32(0, TXFTLR, TFT, value);
        k230_dw_ssi_update_irq(s);
        break;
    case A_RXFTLR:
        s->regs[R_RXFTLR] = FIELD_DP32(0, RXFTLR, RFT, value);
        k230_dw_ssi_update_irq(s);
        break;
    case A_XIP_INCR_INST:
        s->regs[R_XIP_INCR_INST] =
            FIELD_DP32(0, XIP_INCR_INST, INCR_INST, value);
        k230_dw_ssi_update_irq(s);
        break;
    case A_SER:
        s->regs[R_SER] = value & MAKE_64BIT_MASK(0, s->num_cs);
        k230_dw_ssi_update_cs(s);
        break;
    case A_SSIENR:
        s->regs[R_SSIENR] = FIELD_DP32(0, SSIENR, SSIC_EN, value);
        k230_dw_ssi_update_cs(s);
        k230_dw_ssi_update_irq(s);
        break;
    case A_TXEICR:
    case A_RXOICR:
    case A_RXUICR:
    case A_MSTICR:
    case A_ICR:
    case A_XRXOICR:
    case A_TXFLR:
    case A_RXFLR:
    case A_SR:
    case A_ISR:
    case A_RISR:
    case A_IDR:
    case A_SSIC_VERSION_ID:
        break;
    default:
        if (addr >= K230_DW_SSI_REGS_SIZE || (addr & 0x3) != 0) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: bad write offset 0x%" HWADDR_PRIx "\n",
                          DEVICE(s)->canonical_path, addr);
        }
        break;
    }
}

static const MemoryRegionOps k230_dw_ssi_ops = {
    .read = k230_dw_ssi_read,
    .write = k230_dw_ssi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static int k230_dw_ssi_xip_dummy_bytes(uint8_t opcode)
{
    switch (opcode) {
    case 0x0b: /* FAST_READ */
    case 0x6b: /* QOR */
    case 0xeb: /* QIOR */
        return 1;
    default:
        return 0;
    }
}

static int k230_dw_ssi_xip_addr_bytes(uint8_t opcode)
{
    switch (opcode) {
    case 0x0c: /* FAST_READ4 */
    case 0x13: /* READ4 */
    case 0x3c: /* DOR4 */
    case 0x6c: /* QOR4 */
    case 0xbc: /* DIOR4 */
    case 0xec: /* QIOR4 */
        return 4;
    default:
        return 3;
    }
}

static uint64_t k230_dw_ssi_xip_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    K230DwSsiState *s = K230_DW_SSI(opaque);
    uint64_t value = 0;
    uint8_t opcode = FIELD_EX32(s->regs[R_XIP_INCR_INST],
                                XIP_INCR_INST, INCR_INST);
    int addr_bytes;
    int dummy;

    if (!opcode) {
        opcode = K230_DW_SSI_DEFAULT_READ;
    }

    k230_dw_ssi_deselect(s);
    k230_dw_ssi_select(s, 0);
    ssi_transfer(s->spi, opcode);

    addr_bytes = k230_dw_ssi_xip_addr_bytes(opcode);
    for (int i = addr_bytes - 1; i >= 0; i--) {
        ssi_transfer(s->spi, extract32(addr, i * 8, 8));
    }

    dummy = k230_dw_ssi_xip_dummy_bytes(opcode);
    for (int i = 0; i < dummy; i++) {
        ssi_transfer(s->spi, 0);
    }

    for (int i = 0; i < size; i++) {
        value = deposit64(value, i * 8, 8, ssi_transfer(s->spi, 0));
    }

    k230_dw_ssi_deselect(s);
    return value;
}

static void k230_dw_ssi_xip_write(void *opaque, hwaddr addr,
                                  uint64_t value, unsigned int size)
{
    K230DwSsiState *s = K230_DW_SSI(opaque);

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: read-only XIP write at 0x%" HWADDR_PRIx "\n",
                  DEVICE(s)->canonical_path, addr);
}

static const MemoryRegionOps k230_dw_ssi_xip_ops = {
    .read = k230_dw_ssi_xip_read,
    .write = k230_dw_ssi_xip_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = false,
    },
};


static void k230_dw_ssi_enter_reset(Object *obj, ResetType type)
{
    K230DwSsiState *s = K230_DW_SSI(obj);

    memset(s->regs, 0, sizeof(s->regs));
    fifo8_reset(&s->tx_fifo);
    fifo8_reset(&s->rx_fifo);

    s->regs[R_CTRLR0] = K230_DW_SSI_CTRLR0_RESET;
    s->regs[R_SSIC_VERSION_ID] = K230_DW_SSI_VERSION;
    s->regs[R_BAUDR] = 2;
    s->regs[R_XIP_INCR_INST] =
        FIELD_DP32(0, XIP_INCR_INST, INCR_INST, K230_DW_SSI_DEFAULT_READ);

}

static void k230_dw_ssi_hold_reset(Object *obj, ResetType type)
{
    K230DwSsiState *s = K230_DW_SSI(obj);
    s->active_cs = -1;

    if (s->cs_lines) {
        for (int i = 0; i < s->num_cs; i++) {
            qemu_irq_raise(s->cs_lines[i]);
        }
    }

    k230_dw_ssi_update_irq(s);
}

static const VMStateDescription vmstate_k230_dw_ssi = {
    .name = TYPE_K230_DW_SSI,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, K230DwSsiState, K230_DW_SSI_NUM_REGS),
        VMSTATE_FIFO8(tx_fifo, K230DwSsiState),
        VMSTATE_FIFO8(rx_fifo, K230DwSsiState),
        VMSTATE_INT32(active_cs, K230DwSsiState),
        VMSTATE_END_OF_LIST()
    },
};

static void k230_dw_ssi_init(Object *obj)
{
    K230DwSsiState *s = K230_DW_SSI(obj);
    DeviceState *dev = DEVICE(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    s->spi = ssi_create_bus(dev, "spi");
    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->mmio, obj, &k230_dw_ssi_ops, s,
                          TYPE_K230_DW_SSI, K230_DW_SSI_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);

    fifo8_create(&s->tx_fifo, K230_DW_SSI_FIFO_CAPACITY);
    fifo8_create(&s->rx_fifo, K230_DW_SSI_FIFO_CAPACITY);
    s->active_cs = -1;
}

static void k230_dw_ssi_realize(DeviceState *dev, Error **errp)
{
    K230DwSsiState *s = K230_DW_SSI(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    if (s->num_cs == 0 || s->num_cs > 8) {
        error_setg(errp, "%s: num-cs must be in range 1..8",
                   dev->canonical_path);
        return;
    }

    if (s->max_lines != 1 && s->max_lines != 4 && s->max_lines != 8) {
        error_setg(errp, "%s: max-lines must be 1, 4, or 8",
                   dev->canonical_path);
        return;
    }

    if (s->xip.enabled && s->xip.window_size == 0) {
        error_setg(errp, "%s: XIP window size must be non-zero",
                   dev->canonical_path);
        return;
    }

    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    qdev_init_gpio_out_named(dev, s->cs_lines, "cs", s->num_cs);

    if (s->xip.enabled) {
        memory_region_init_io(&s->xip.mmio, OBJECT(s),
                              &k230_dw_ssi_xip_ops, s,
                              "k230.dw-ssi.xip", s->xip.window_size);
        sysbus_init_mmio(sbd, &s->xip.mmio);
    }
}

static void k230_dw_ssi_finalize(Object *obj)
{
    K230DwSsiState *s = K230_DW_SSI(obj);

    fifo8_destroy(&s->tx_fifo);
    fifo8_destroy(&s->rx_fifo);
    g_free(s->cs_lines);
}

static const Property k230_dw_ssi_properties[] = {
    DEFINE_PROP_UINT32("num-cs", K230DwSsiState, num_cs, 1),
    DEFINE_PROP_UINT32("max-lines", K230DwSsiState, max_lines, 1),
    DEFINE_PROP_BOOL("has-xip", K230DwSsiState, xip.enabled, false),
    DEFINE_PROP_SIZE("flash-window-size", K230DwSsiState, xip.window_size, 0),
};

static void k230_dw_ssi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = k230_dw_ssi_realize;
    dc->vmsd = &vmstate_k230_dw_ssi;
    device_class_set_props(dc, k230_dw_ssi_properties);
    rc->phases.enter = k230_dw_ssi_enter_reset;
    rc->phases.hold = k230_dw_ssi_hold_reset;
}

static const TypeInfo k230_dw_ssi_info = {
    .name = TYPE_K230_DW_SSI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(K230DwSsiState),
    .instance_init = k230_dw_ssi_init,
    .instance_finalize = k230_dw_ssi_finalize,
    .class_init = k230_dw_ssi_class_init,
};

static void k230_dw_ssi_register_types(void)
{
    type_register_static(&k230_dw_ssi_info);
}

type_init(k230_dw_ssi_register_types)
