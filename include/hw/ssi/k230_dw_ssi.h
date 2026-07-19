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
/* Patch 9：spi0 的 Flash memory-mapped read window（128 MiB）。 */
#define K230_DW_SSI_XIP_WINDOW_SIZE 0x08000000

typedef struct K230HiSysState K230HiSysState;

/*
 * LEARNING(P6): 外部 IRQ 顺序来自 K230 SSI 集成接口，不等同于
 * RISR/ISR 的位号。PLIC source ID 属于 machine 接线，不应出现在这里。
 */
typedef enum K230DwSsiIrq {
    K230_DW_SSI_IRQ_TXE,
    K230_DW_SSI_IRQ_TXO,
    K230_DW_SSI_IRQ_RXF,
    K230_DW_SSI_IRQ_RXO,
    K230_DW_SSI_IRQ_TXU,
    K230_DW_SSI_IRQ_RXU,
    K230_DW_SSI_IRQ_MST,
    K230_DW_SSI_IRQ_DONE,
    K230_DW_SSI_IRQ_AXIE,
    K230_DW_SSI_IRQ_COUNT,
} K230DwSsiIrq;

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

    /*
     * LEARNING(P5): Enhanced SPI 不是第二套 FIFO 引擎。以下阶段只描述
     * 同一条事务在 instruction/address/mode/dummy/data 间的推进位置。
     */
    K230_DW_SSI_PHASE_ENHANCED_INSTRUCTION,
    K230_DW_SSI_PHASE_ENHANCED_ADDRESS,
    K230_DW_SSI_PHASE_ENHANCED_MODE,
    K230_DW_SSI_PHASE_ENHANCED_DUMMY,
    K230_DW_SSI_PHASE_ENHANCED_DATA,
} K230DwSsiPhase;

/*
 * LEARNING(P5): 这是“增强事务描述”，不是 Flash opcode 状态机。
 * PIO 和后续 XIP 都应先生成同一种描述，再由统一阶段执行器发送。
 */
typedef struct K230DwSsiEnhancedCommand {
    uint32_t instruction;
    uint32_t address;
    uint32_t mode;
    uint32_t instruction_bits;
    uint32_t address_bits;
    uint32_t mode_bits;
    uint32_t wait_cycles;
    uint32_t data_frames;
    uint32_t spi_frf;
    uint32_t trans_type;
    uint32_t tmod;
    bool mode_bits_enabled;
} K230DwSsiEnhancedCommand;


struct K230DwSsiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    /* 第二个 SysBus MMIO：不是寄存器，而是 guest 读取 Flash 的 XIP 窗口。 */
    MemoryRegion xip;
    SSIBus *spi;
    /* 仅 spi0 设置；HI_SYS 的 XIP_EN 是这个窗口的总开关。 */
    K230HiSysState *hi_sys;
    qemu_irq *cs_lines;
    qemu_irq irqs[K230_DW_SSI_IRQ_COUNT];

    Fifo32 tx_fifo;
    Fifo32 rx_fifo;
    uint32_t regs[K230_DW_SSI_NUM_REGS];

    /* LEARNING(P6): 只保存错误锁存；TXE/RXF 必须从 FIFO 水位动态计算。 */
    uint32_t irq_latched;

    uint32_t phase;
    uint32_t remaining_frames;
    K230DwSsiEnhancedCommand enhanced;

    uint32_t num_cs;
    uint32_t max_lines;
    int active_cs;
    bool sleep_status;
};

uint32_t k230_dw_ssi_get_spi_mode(const K230DwSsiState *s);
bool k230_dw_ssi_is_sleeping(const K230DwSsiState *s);
void k230_dw_ssi_set_hi_sys(K230DwSsiState *s, K230HiSysState *hi_sys);

#endif /* HW_SSI_K230_DW_SSI_H */
