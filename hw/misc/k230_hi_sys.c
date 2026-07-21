/*
 * K230 HI_SYS_CONFIG SSI wrapper
 *
 * SSI_CTRL is a SoC wrapper register and must not be folded into any DWC
 * SSI controller's DR2 register at base + 0x068.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/misc/k230_hi_sys.h"
#include "migration/vmstate.h"

static uint32_t k230_hi_sys_ssi_status(const K230HiSysState *s)
{
    static const unsigned int sleep_bits[] = {
        K230_SSI_CTRL_SPI0_SLEEP,
        K230_SSI_CTRL_SPI1_SLEEP,
        K230_SSI_CTRL_SPI2_SLEEP,
    };
    static const unsigned int mode_shifts[] = {
        K230_SSI_CTRL_SPI0_MODE_SHIFT,
        K230_SSI_CTRL_SPI1_MODE_SHIFT,
        K230_SSI_CTRL_SPI2_MODE_SHIFT,
    };
    uint32_t value = s->ssi_ctrl;

    for (unsigned int i = 0; i < ARRAY_SIZE(s->ssi); i++) {
        if (!s->ssi[i]) {
            continue;
        }

        value |= k230_dw_ssi_get_spi_mode(s->ssi[i]) << mode_shifts[i];
        if (k230_dw_ssi_is_sleeping(s->ssi[i])) {
            value |= sleep_bits[i];
        }
    }

    return value;
}

static uint64_t k230_hi_sys_read(void *opaque, hwaddr addr, unsigned size)
{
    K230HiSysState *s = opaque;

    if (addr == K230_HI_SYS_SSI_CTRL_OFFSET) {
        return k230_hi_sys_ssi_status(s);
    }

    if (addr < K230_HI_SYS_MMIO_SIZE) {
        return 0;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                  TYPE_K230_HI_SYS, addr);
    return 0;
}

static void k230_hi_sys_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned size)
{
    K230HiSysState *s = opaque;

    if (addr == K230_HI_SYS_SSI_CTRL_OFFSET) {
        s->ssi_ctrl = (s->ssi_ctrl & ~K230_SSI_CTRL_WRITABLE_MASK) |
                      ((uint32_t)value & K230_SSI_CTRL_WRITABLE_MASK);
        s->ssi_ctrl &= K230_SSI_CTRL_IMPLEMENTED_MASK;
        return;
    }

    if (addr < K230_HI_SYS_MMIO_SIZE) {
        return;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                  TYPE_K230_HI_SYS, addr);
}

static const MemoryRegionOps k230_hi_sys_ops = {
    .read = k230_hi_sys_read,
    .write = k230_hi_sys_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

void k230_hi_sys_set_ssi(K230HiSysState *s, unsigned int index,
                         K230DwSsiState *ssi)
{
    g_assert(index < ARRAY_SIZE(s->ssi));
    s->ssi[index] = ssi;
}

bool k230_hi_sys_xip_enabled(const K230HiSysState *s)
{
    return !!(s->ssi_ctrl & K230_SSI_CTRL_XIP_EN);
}

static void k230_hi_sys_reset(Object *obj, ResetType type)
{
    K230HiSysState *s = K230_HI_SYS(obj);

    s->ssi_ctrl = K230_SSI_CTRL_RESET;
}

static void k230_hi_sys_init(Object *obj)
{
    K230HiSysState *s = K230_HI_SYS(obj);

    memory_region_init_io(&s->mmio, obj, &k230_hi_sys_ops, s,
                          TYPE_K230_HI_SYS, K230_HI_SYS_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static const VMStateDescription vmstate_k230_hi_sys = {
    .name = TYPE_K230_HI_SYS,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(ssi_ctrl, K230HiSysState),
        VMSTATE_END_OF_LIST()
    },
};

static void k230_hi_sys_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->vmsd = &vmstate_k230_hi_sys;
    rc->phases.enter = k230_hi_sys_reset;
}

static const TypeInfo k230_hi_sys_type_info = {
    .name = TYPE_K230_HI_SYS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(K230HiSysState),
    .instance_init = k230_hi_sys_init,
    .class_init = k230_hi_sys_class_init,
};

static void k230_hi_sys_register_types(void)
{
    type_register_static(&k230_hi_sys_type_info);
}

type_init(k230_hi_sys_register_types)
