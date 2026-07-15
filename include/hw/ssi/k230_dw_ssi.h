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

/*
 * Patch 3 学习脚手架只描述 Standard SPI 的软件可见阶段。
 * Enhanced SPI/QSPI 阶段在后续 Patch 中扩展，不能提前混入这里。
 */
typedef enum K230DwSsiPhase {
    /* LEARNING(P3): 当前没有需要跨 MMIO 访问保存的传输进度。 */
    K230_DW_SSI_PHASE_IDLE,

    /*
     * LEARNING(P3): RX-only 已由一个 dummy DR 启动，控制器正在生成
     * NDF + 1 个接收帧；RX FIFO 满时可以暂停在这个阶段。
     */
    K230_DW_SSI_PHASE_RX_ONLY,

    /*
     * LEARNING(P3): EEPROM_READ 的发送阶段。TX FIFO 中保存 opcode、
     * address 等控制帧，这一阶段线路返回值不进入 RX FIFO。
     */
    K230_DW_SSI_PHASE_EEPROM_COMMAND,

    /*
     * LEARNING(P3): EEPROM_READ 的接收阶段。命令发送完毕后，控制器
     * 使用 dummy 帧继续产生时钟，并接收 NDF + 1 个数据帧。
     */
    K230_DW_SSI_PHASE_EEPROM_DATA,
} K230DwSsiPhase;

struct K230DwSsiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    SSIBus *spi;
    qemu_irq *cs_lines;

    Fifo32 tx_fifo;
    Fifo32 rx_fifo;
    uint32_t regs[K230_DW_SSI_NUM_REGS];

    /* LEARNING(P3): Standard SPI 当前处于哪个跨调用阶段。 */
    int32_t phase;

    /* LEARNING(P3): RO/EEPROM 数据阶段尚未产生的接收帧数。 */
    uint32_t remaining_frames;

    /*
     * LEARNING(P3): BUSY 描述传输引擎是否仍有未完成工作，不能直接
     * 使用 SSIENR 或 SER 推导，因为“控制器使能/CS 有效”不等于忙。
     */
    bool busy;

    uint32_t num_cs;
    uint32_t max_lines;
    int active_cs;
};

#endif /* HW_SSI_K230_DW_SSI_H */
