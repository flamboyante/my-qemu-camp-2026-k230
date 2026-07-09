/*
 * K230 DWC SSI compatible SPI/QSPI controller
 *
 * This model implements the register/FIFO/SSI paths needed by K230
 * controller-level tests and a read-only SPI NOR XIP window.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
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

#define K230_DW_SSI_SSIENR_ENABLE BIT(0)

#define K230_DW_SSI_SR_BUSY BIT(0)
#define K230_DW_SSI_SR_TFNF BIT(1)
#define K230_DW_SSI_SR_TFE  BIT(2)
#define K230_DW_SSI_SR_RFNE BIT(3)
#define K230_DW_SSI_SR_RFF  BIT(4)

#define K230_DW_SSI_INT_TXEI BIT(0)
#define K230_DW_SSI_INT_RXFI BIT(2)

static unsigned k230_dw_ssi_reg_index(hwaddr addr)
{
    return addr / sizeof(uint32_t);
}

static uint32_t k230_dw_ssi_get_reg(K230DwSsiState *s, hwaddr addr)
{
    return s->regs[k230_dw_ssi_reg_index(addr)];
}

static void k230_dw_ssi_set_reg(K230DwSsiState *s, hwaddr addr, uint32_t value)
{
    s->regs[k230_dw_ssi_reg_index(addr)] = value;
}

static bool k230_dw_ssi_enabled(K230DwSsiState *s)
{
    return k230_dw_ssi_get_reg(s, K230_DW_SSI_SSIENR) &
           K230_DW_SSI_SSIENR_ENABLE;
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
    uint32_t ser = k230_dw_ssi_get_reg(s, K230_DW_SSI_SER);

    if (!k230_dw_ssi_enabled(s) || !ser) {
        k230_dw_ssi_deselect(s);
        return;
    }

    k230_dw_ssi_select(s, ctz32(ser));
}

static uint32_t k230_dw_ssi_raw_irq(K230DwSsiState *s)
{
    uint32_t raw = 0;

    if (fifo8_num_used(&s->tx_fifo) <=
        k230_dw_ssi_get_reg(s, K230_DW_SSI_TXFTLR)) {
        raw |= K230_DW_SSI_INT_TXEI;
    }

    if (fifo8_num_used(&s->rx_fifo) >
        k230_dw_ssi_get_reg(s, K230_DW_SSI_RXFTLR)) {
        raw |= K230_DW_SSI_INT_RXFI;
    }

    return raw;
}

static void k230_dw_ssi_update_irq(K230DwSsiState *s)
{
    uint32_t level = k230_dw_ssi_raw_irq(s) &
                     k230_dw_ssi_get_reg(s, K230_DW_SSI_IMR);

    qemu_set_irq(s->irq, !!level);
}

static uint32_t k230_dw_ssi_status(K230DwSsiState *s)
{
    uint32_t tx_used = fifo8_num_used(&s->tx_fifo);
    uint32_t rx_used = fifo8_num_used(&s->rx_fifo);
    uint32_t sr = 0;

    if (tx_used < K230_DW_SSI_FIFO_CAPACITY) {
        sr |= K230_DW_SSI_SR_TFNF;
    }
    if (tx_used == 0) {
        sr |= K230_DW_SSI_SR_TFE;
    }
    if (rx_used != 0) {
        sr |= K230_DW_SSI_SR_RFNE;
    }
    if (rx_used == K230_DW_SSI_FIFO_CAPACITY) {
        sr |= K230_DW_SSI_SR_RFF;
    }

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
    return addr >= K230_DW_SSI_DR0 && addr <= K230_DW_SSI_DR_END &&
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
    case K230_DW_SSI_TXFLR:
        value = fifo8_num_used(&s->tx_fifo);
        break;
    case K230_DW_SSI_RXFLR:
        value = fifo8_num_used(&s->rx_fifo);
        break;
    case K230_DW_SSI_SR:
        value = k230_dw_ssi_status(s);
        break;
    case K230_DW_SSI_RISR:
        value = k230_dw_ssi_raw_irq(s);
        break;
    case K230_DW_SSI_ISR:
        value = k230_dw_ssi_raw_irq(s) &
                k230_dw_ssi_get_reg(s, K230_DW_SSI_IMR);
        break;
    case K230_DW_SSI_TXEICR:
    case K230_DW_SSI_RXOICR:
    case K230_DW_SSI_RXUICR:
    case K230_DW_SSI_MSTICR:
    case K230_DW_SSI_ICR:
    case K230_DW_SSI_XRXOICR:
        value = 0;
        break;
    case K230_DW_SSI_SSIC_VERSION_ID:
        value = K230_DW_SSI_VERSION;
        break;
    case K230_DW_SSI_IDR:
        value = 0;
        break;
    default:
        if (addr < K230_DW_SSI_MMIO_SIZE && (addr & 0x3) == 0) {
            value = k230_dw_ssi_get_reg(s, addr);
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
    case K230_DW_SSI_CTRLR0:
    case K230_DW_SSI_CTRLR1:
    case K230_DW_SSI_MWCR:
    case K230_DW_SSI_BAUDR:
    case K230_DW_SSI_TXFTLR:
    case K230_DW_SSI_RXFTLR:
    case K230_DW_SSI_IMR:
    case K230_DW_SSI_DMACR:
    case K230_DW_SSI_DMATDLR:
    case K230_DW_SSI_DMARDLR:
    case K230_DW_SSI_RX_SAMPLE_DELAY:
    case K230_DW_SSI_SPI_CTRLR0:
    case K230_DW_SSI_TXD_DRIVE_EDGE:
    case K230_DW_SSI_XIP_MODE_BITS:
    case K230_DW_SSI_XIP_INCR_INST:
    case K230_DW_SSI_XIP_WRAP_INST:
    case K230_DW_SSI_XIP_CTRL:
    case K230_DW_SSI_XIP_SER:
    case K230_DW_SSI_XIP_CNT_TIME_OUT:
    case K230_DW_SSI_SPI_CTRLR1:
        k230_dw_ssi_set_reg(s, addr, value);
        k230_dw_ssi_update_irq(s);
        break;
    case K230_DW_SSI_SER:
        k230_dw_ssi_set_reg(s, addr, value & MAKE_64BIT_MASK(0, s->num_cs));
        k230_dw_ssi_update_cs(s);
        break;
    case K230_DW_SSI_SSIENR:
        k230_dw_ssi_set_reg(s, addr, value & K230_DW_SSI_SSIENR_ENABLE);
        k230_dw_ssi_update_cs(s);
        k230_dw_ssi_update_irq(s);
        break;
    case K230_DW_SSI_TXEICR:
    case K230_DW_SSI_RXOICR:
    case K230_DW_SSI_RXUICR:
    case K230_DW_SSI_MSTICR:
    case K230_DW_SSI_ICR:
    case K230_DW_SSI_XRXOICR:
    case K230_DW_SSI_TXFLR:
    case K230_DW_SSI_RXFLR:
    case K230_DW_SSI_SR:
    case K230_DW_SSI_ISR:
    case K230_DW_SSI_RISR:
    case K230_DW_SSI_IDR:
    case K230_DW_SSI_SSIC_VERSION_ID:
        break;
    default:
        if (addr >= K230_DW_SSI_MMIO_SIZE || (addr & 0x3) != 0) {
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
    uint8_t opcode = k230_dw_ssi_get_reg(s, K230_DW_SSI_XIP_INCR_INST) & 0xff;
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

static void k230_dw_ssi_reset(DeviceState *dev)
{
    K230DwSsiState *s = K230_DW_SSI(dev);

    memset(s->regs, 0, sizeof(s->regs));
    fifo8_reset(&s->tx_fifo);
    fifo8_reset(&s->rx_fifo);

    k230_dw_ssi_set_reg(s, K230_DW_SSI_CTRLR0, K230_DW_SSI_CTRLR0_RESET);
    k230_dw_ssi_set_reg(s, K230_DW_SSI_SSIC_VERSION_ID, K230_DW_SSI_VERSION);
    k230_dw_ssi_set_reg(s, K230_DW_SSI_BAUDR, 2);
    k230_dw_ssi_set_reg(s, K230_DW_SSI_XIP_INCR_INST,
                        K230_DW_SSI_DEFAULT_READ);

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
        VMSTATE_UINT32_ARRAY(regs, K230DwSsiState,
                             K230_DW_SSI_MMIO_SIZE / sizeof(uint32_t)),
        VMSTATE_FIFO8(tx_fifo, K230DwSsiState),
        VMSTATE_FIFO8(rx_fifo, K230DwSsiState),
        VMSTATE_INT32(active_cs, K230DwSsiState),
        VMSTATE_END_OF_LIST()
    },
};

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

    if (s->has_xip && s->flash_window_size == 0) {
        error_setg(errp, "%s: XIP window size must be non-zero",
                   dev->canonical_path);
        return;
    }

    s->spi = ssi_create_bus(dev, "spi");
    sysbus_init_irq(sbd, &s->irq);

    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    qdev_init_gpio_out_named(dev, s->cs_lines, "cs", s->num_cs);

    memory_region_init_io(&s->mmio, OBJECT(s), &k230_dw_ssi_ops, s,
                          TYPE_K230_DW_SSI, K230_DW_SSI_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);

    if (s->has_xip) {
        memory_region_init_io(&s->xip, OBJECT(s), &k230_dw_ssi_xip_ops, s,
                              "k230.dw-ssi.xip", s->flash_window_size);
        sysbus_init_mmio(sbd, &s->xip);
    }

    fifo8_create(&s->tx_fifo, K230_DW_SSI_FIFO_CAPACITY);
    fifo8_create(&s->rx_fifo, K230_DW_SSI_FIFO_CAPACITY);
    s->active_cs = -1;
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
    DEFINE_PROP_BOOL("has-xip", K230DwSsiState, has_xip, false),
    DEFINE_PROP_SIZE("flash-window-size", K230DwSsiState, flash_window_size, 0),
};

static void k230_dw_ssi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = k230_dw_ssi_realize;
    dc->vmsd = &vmstate_k230_dw_ssi;
    device_class_set_props(dc, k230_dw_ssi_properties);
    device_class_set_legacy_reset(dc, k230_dw_ssi_reset);
}

static const TypeInfo k230_dw_ssi_info = {
    .name = TYPE_K230_DW_SSI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(K230DwSsiState),
    .instance_finalize = k230_dw_ssi_finalize,
    .class_init = k230_dw_ssi_class_init,
};

static void k230_dw_ssi_register_types(void)
{
    type_register_static(&k230_dw_ssi_info);
}

type_init(k230_dw_ssi_register_types)
