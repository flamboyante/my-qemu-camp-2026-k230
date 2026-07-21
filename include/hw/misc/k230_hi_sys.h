/*
 * K230 HI_SYS_CONFIG SSI wrapper
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_K230_HI_SYS_H
#define HW_MISC_K230_HI_SYS_H

#include "hw/core/sysbus.h"
#include "hw/ssi/k230_dw_ssi.h"
#include "qom/object.h"

#define TYPE_K230_HI_SYS "riscv.k230.hi-sys"
OBJECT_DECLARE_SIMPLE_TYPE(K230HiSysState, K230_HI_SYS)

#define K230_HI_SYS_MMIO_SIZE            0x400
#define K230_HI_SYS_SSI_CTRL_OFFSET      0x068

#define K230_SSI_CTRL_RESET              0x00004000U
#define K230_SSI_CTRL_IMPLEMENTED_MASK   0x0003fff1U
#define K230_SSI_CTRL_WRITABLE_MASK      0x0003e001U

#define K230_SSI_CTRL_XIP_EN             (1U << 0)
#define K230_SSI_CTRL_SPI0_SLEEP         (1U << 4)
#define K230_SSI_CTRL_SPI0_MODE_SHIFT    5
#define K230_SSI_CTRL_SPI1_SLEEP         (1U << 7)
#define K230_SSI_CTRL_SPI1_MODE_SHIFT    8
#define K230_SSI_CTRL_SPI2_SLEEP         (1U << 10)
#define K230_SSI_CTRL_SPI2_MODE_SHIFT    11

struct K230HiSysState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    uint32_t ssi_ctrl;
    K230DwSsiState *ssi[3];
};

void k230_hi_sys_set_ssi(K230HiSysState *s, unsigned int index,
                         K230DwSsiState *ssi);

#endif /* HW_MISC_K230_HI_SYS_H */
