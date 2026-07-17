/*
 * K230 DWC SSI compatible SPI/QSPI controller
 *
 * This model implements the K230 register, FIFO, and SSI transfer paths.
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

#define K230_DW_SSI_FIFO_CAPACITY 256

#define K230_DW_SSI_CTRLR0_RESET            0x00004007
#define K230_DW_SSI_SR_RESET                0x00000006
#define K230_DW_SSI_IMR_RESET               0x0000003f
#define K230_DW_SSI_IDR_RESET               0xa1b2c3d5
#define K230_DW_SSI_SPI_CTRLR0_SPI_RESET    0x04000200
#define K230_DW_SSI_SPI_CTRLR0_FMC_RESET    0x28000200
#define K230_DW_SSI_AXIAWLEN_RESET          0x00000700
#define K230_DW_SSI_AXIARLEN_RESET          0x00000700
#define K230_DW_SSI_VERSION                 0x3130332a

/*
 * LEARNING(P5): Patch 5 的临时中文脚手架可通过下面的命令统一定位：
 *
 *   rg -n "LEARNING\(P5|TODO\(P5" hw/ssi include/hw/ssi tests/qtest
 */

/*
 * LEARNING(P3): 这些中文注释是临时学习脚手架。
 * Patch 3 完成后可使用下面的命令定位并统一清理：
 *
 *   rg -n "LEARNING\(P3" hw/ssi include/hw/ssi tests/qtest
 */

REG32(CTRLR0, 0x000)
    FIELD(CTRLR0, DFS, 0, 5)
    FIELD(CTRLR0, FRF, 6, 2)
    FIELD(CTRLR0, SCPH, 8, 1)
    FIELD(CTRLR0, SCPOL, 9, 1)
    FIELD(CTRLR0, TMOD, 10, 2)
    FIELD(CTRLR0, SLV_OE, 12, 1)
    FIELD(CTRLR0, SRL, 13, 1)
    FIELD(CTRLR0, SSTE, 14, 1)
    FIELD(CTRLR0, CFS, 16, 4)
    FIELD(CTRLR0, SPI_FRF, 22, 2)
    FIELD(CTRLR0, SPI_HYPERBUS_EN, 24, 1)
    FIELD(CTRLR0, SPI_DWS_EN, 25, 1)
REG32(CTRLR1, 0x004)
    FIELD(CTRLR1, NDF, 0, 16)
REG32(SSIENR, 0x008)
    FIELD(SSIENR, SSIC_EN, 0, 1)
REG32(MWCR, 0x00c)
    FIELD(MWCR, MWMOD, 0, 1)
    FIELD(MWCR, MDD, 1, 1)
    FIELD(MWCR, MHS, 2, 1)
REG32(SER, 0x010)
REG32(BAUDR, 0x014)
    FIELD(BAUDR, SCKDV, 1, 15)
REG32(TXFTLR, 0x018)
    FIELD(TXFTLR, TFT, 0, 8)
    FIELD(TXFTLR, TXFTHR, 16, 11)
REG32(RXFTLR, 0x01c)
    FIELD(RXFTLR, RFT, 0, 8)
REG32(TXFLR, 0x020)
    FIELD(TXFLR, TXTFL, 0, 9)
REG32(RXFLR, 0x024)
    FIELD(RXFLR, RXTFL, 0, 9)
REG32(SR, 0x028)
    FIELD(SR, CMPLTD_DF, 15, 17)
    FIELD(SR, DCOL, 6, 1)
    FIELD(SR, TXE, 5, 1)
    FIELD(SR, BUSY, 0, 1)
    FIELD(SR, TFNF, 1, 1)
    FIELD(SR, TFE, 2, 1)
    FIELD(SR, RFNE, 3, 1)
    FIELD(SR, RFF, 4, 1)
REG32(IMR, 0x02c)
    FIELD(IMR, DONEM, 11, 1)
    FIELD(IMR, SPITEM, 10, 1)
    FIELD(IMR, AXIEM, 8, 1)
    FIELD(IMR, TXUIM, 7, 1)
    FIELD(IMR, XRXOIM, 6, 1)
    FIELD(IMR, MSTIM, 5, 1)
    FIELD(IMR, RXOIM, 3, 1)
    FIELD(IMR, RXUIM, 2, 1)
    FIELD(IMR, TXOIM, 1, 1)
    FIELD(IMR, TXEIM, 0, 1)
    FIELD(IMR, RXFIM, 4, 1)
REG32(ISR, 0x030)
    FIELD(ISR, DONES, 11, 1)
    FIELD(ISR, SPITES, 10, 1)
    FIELD(ISR, AXIES, 8, 1)
    FIELD(ISR, TXUIS, 7, 1)
    FIELD(ISR, XRXOIS, 6, 1)
    FIELD(ISR, MSTIS, 5, 1)
    FIELD(ISR, RXOIS, 3, 1)
    FIELD(ISR, RXUIS, 2, 1)
    FIELD(ISR, TXOIS, 1, 1)
    FIELD(ISR, TXEIS, 0, 1)
    FIELD(ISR, RXFIS, 4, 1)
REG32(RISR, 0x034)
    FIELD(RISR, DONER, 11, 1)
    FIELD(RISR, SPITER, 10, 1)
    FIELD(RISR, AXIER, 8, 1)
    FIELD(RISR, TXUIR, 7, 1)
    FIELD(RISR, XRXOIR, 6, 1)
    FIELD(RISR, MSTIR, 5, 1)
    FIELD(RISR, RXOIR, 3, 1)
    FIELD(RISR, RXUIR, 2, 1)
    FIELD(RISR, TXOIR, 1, 1)
    FIELD(RISR, TXEIR, 0, 1)
    FIELD(RISR, RXFIR, 4, 1)
REG32(TXEICR, 0x038)
    FIELD(TXEICR, TXEICR, 0, 1)
REG32(RXOICR, 0x03c)
    FIELD(RXOICR, RXOICR, 0, 1)
REG32(RXUICR, 0x040)
    FIELD(RXUICR, RXUICR, 0, 1)
REG32(MSTICR, 0x044)
    FIELD(MSTICR, MSTICR, 0, 1)
REG32(ICR, 0x048)
    FIELD(ICR, ICR, 0, 1)
REG32(DMACR, 0x04c)
    FIELD(DMACR, RDMAE, 0, 1)
    FIELD(DMACR, TDMAE, 1, 1)
    FIELD(DMACR, IDMAE, 2, 1)
    FIELD(DMACR, ATW, 3, 2)
    FIELD(DMACR, AINC, 6, 1)
    FIELD(DMACR, ACACHE, 8, 4)
    FIELD(DMACR, APROT, 12, 3)
    FIELD(DMACR, AID, 15, 4)
REG32(AXIAWLEN, 0x050)
    FIELD(AXIAWLEN, AWLEN, 8, 8)
REG32(AXIARLEN, 0x054)
    FIELD(AXIARLEN, ARLEN, 8, 8)
REG32(IDR, 0x058)
    FIELD(IDR, IDCODE, 0, 32)
REG32(SSIC_VERSION_ID, 0x05c)
    FIELD(SSIC_VERSION_ID, VERSION_ID, 0, 32)
REG32(DR0, 0x060)
REG32(DR_END, 0x0ec)
REG32(RX_SAMPLE_DELAY, 0x0f0)
    FIELD(RX_SAMPLE_DELAY, RSD, 0, 8)
    FIELD(RX_SAMPLE_DELAY, SE, 16, 1)
REG32(SPI_CTRLR0, 0x0f4)
    FIELD(SPI_CTRLR0, CLK_STRETCH_EN, 30, 1)
    FIELD(SPI_CTRLR0, XIP_PREFETCH_EN, 29, 1)
    FIELD(SPI_CTRLR0, XIP_MBL, 26, 2)
    FIELD(SPI_CTRLR0, SPI_RXDS_SIG_EN, 25, 1)
    FIELD(SPI_CTRLR0, SPI_DM_EN, 24, 1)
    FIELD(SPI_CTRLR0, SSIC_XIP_CONT_XFER_EN, 21, 1)
    FIELD(SPI_CTRLR0, XIP_INST_EN, 20, 1)
    FIELD(SPI_CTRLR0, XIP_DFS_HC, 19, 1)
    FIELD(SPI_CTRLR0, SPI_RXDS_EN, 18, 1)
    FIELD(SPI_CTRLR0, INST_DDR_EN, 17, 1)
    FIELD(SPI_CTRLR0, SPI_DDR_EN, 16, 1)
    FIELD(SPI_CTRLR0, WAIT_CYCLES, 11, 5)
    FIELD(SPI_CTRLR0, INST_L, 8, 2)
    FIELD(SPI_CTRLR0, XIP_MD_BIT_EN, 7, 1)
    FIELD(SPI_CTRLR0, ADDR_L, 2, 4)
    FIELD(SPI_CTRLR0, TRANS_TYPE, 0, 2)
REG32(DDR_DRIVE_EDGE, 0x0f8)
    FIELD(DDR_DRIVE_EDGE, TDE, 0, 8)
REG32(XIP_MODE_BITS, 0x0fc)
    FIELD(XIP_MODE_BITS, XIP_MD_BITS, 0, 16)
REG32(XIP_INCR_INST, 0x100)
    FIELD(XIP_INCR_INST, INCR_INST, 0, 16)
REG32(XIP_WRAP_INST, 0x104)
    FIELD(XIP_WRAP_INST, WRAP_INST, 0, 16)
REG32(XIP_CTRL, 0x108)
REG32(XIP_SER, 0x10c)
REG32(XRXOICR, 0x110)
REG32(XIP_CNT_TIME_OUT, 0x114)
REG32(SPI_CTRLR1, 0x118)
REG32(SPITECR, 0x11c)
REG32(SPIDR, 0x120)
    FIELD(SPIDR, SPI_INST, 0, 16)
REG32(SPIAR, 0x124)
    FIELD(SPIAR, SDAR, 0, 32)
REG32(AXIAR0, 0x128)
    FIELD(AXIAR0, AXIAR_0_31, 0, 32)
REG32(AXIAR1, 0x12c)
    FIELD(AXIAR1, AXIAR_32_63, 0, 32)
REG32(AXIECR, 0x130)
    FIELD(AXIECR, AXIECR, 0, 1)
REG32(DONECR, 0x134)
    FIELD(DONECR, DONECR, 0, 1)
REG32(RSVD_138, 0x138)
REG32(RSVD_13C, 0x13c)
REG32(XIP_WRITE_INCR_INST, 0x140)
REG32(XIP_WRITE_WRAP_INST, 0x144)
REG32(XIP_WRITE_CTRL, 0x148)

#define K230_DW_SSI_CTRLR0_WRITABLE_MASK \
    (R_CTRLR0_DFS_MASK | \
     R_CTRLR0_SCPH_MASK | \
     R_CTRLR0_SCPOL_MASK | \
     R_CTRLR0_TMOD_MASK | \
     R_CTRLR0_SLV_OE_MASK | \
     R_CTRLR0_SRL_MASK | \
     R_CTRLR0_SSTE_MASK | \
     R_CTRLR0_CFS_MASK | \
     R_CTRLR0_SPI_FRF_MASK | \
     R_CTRLR0_SPI_HYPERBUS_EN_MASK)

#define K230_DW_SSI_CTRLR1_WRITABLE_MASK R_CTRLR1_NDF_MASK
#define K230_DW_SSI_MWCR_WRITABLE_MASK \
    (R_MWCR_MWMOD_MASK | R_MWCR_MDD_MASK)
#define K230_DW_SSI_BAUDR_WRITABLE_MASK R_BAUDR_SCKDV_MASK
#define K230_DW_SSI_TXFTLR_WRITABLE_MASK \
    (R_TXFTLR_TFT_MASK | R_TXFTLR_TXFTHR_MASK)
#define K230_DW_SSI_RXFTLR_WRITABLE_MASK R_RXFTLR_RFT_MASK
#define K230_DW_SSI_DMACR_WRITABLE_MASK \
    (R_DMACR_IDMAE_MASK | R_DMACR_ATW_MASK | R_DMACR_AINC_MASK | \
     R_DMACR_ACACHE_MASK | R_DMACR_APROT_MASK | R_DMACR_AID_MASK)
#define K230_DW_SSI_AXIAWLEN_WRITABLE_MASK R_AXIAWLEN_AWLEN_MASK
#define K230_DW_SSI_AXIARLEN_WRITABLE_MASK R_AXIARLEN_ARLEN_MASK
#define K230_DW_SSI_IMR_WRITABLE_MASK \
    (R_IMR_TXEIM_MASK | R_IMR_TXOIM_MASK | R_IMR_RXUIM_MASK | \
     R_IMR_RXOIM_MASK | R_IMR_RXFIM_MASK | R_IMR_MSTIM_MASK | \
     R_IMR_TXUIM_MASK | R_IMR_AXIEM_MASK | R_IMR_DONEM_MASK)
#define K230_DW_SSI_RX_SAMPLE_DELAY_WRITABLE_MASK \
    (R_RX_SAMPLE_DELAY_RSD_MASK | R_RX_SAMPLE_DELAY_SE_MASK)
#define K230_DW_SSI_SPI_CTRLR0_WRITABLE_MASK \
    (R_SPI_CTRLR0_CLK_STRETCH_EN_MASK | \
     R_SPI_CTRLR0_XIP_PREFETCH_EN_MASK | \
     R_SPI_CTRLR0_XIP_MBL_MASK | \
     R_SPI_CTRLR0_SPI_RXDS_SIG_EN_MASK | \
     R_SPI_CTRLR0_SPI_DM_EN_MASK | \
     R_SPI_CTRLR0_SSIC_XIP_CONT_XFER_EN_MASK | \
     R_SPI_CTRLR0_XIP_INST_EN_MASK | \
     R_SPI_CTRLR0_XIP_DFS_HC_MASK | \
     R_SPI_CTRLR0_INST_DDR_EN_MASK | \
     R_SPI_CTRLR0_SPI_DDR_EN_MASK | \
     R_SPI_CTRLR0_SPI_RXDS_EN_MASK | \
     R_SPI_CTRLR0_WAIT_CYCLES_MASK | \
     R_SPI_CTRLR0_INST_L_MASK | \
     R_SPI_CTRLR0_XIP_MD_BIT_EN_MASK | \
     R_SPI_CTRLR0_ADDR_L_MASK | \
     R_SPI_CTRLR0_TRANS_TYPE_MASK)
#define K230_DW_SSI_DDR_DRIVE_EDGE_WRITABLE_MASK \
    R_DDR_DRIVE_EDGE_TDE_MASK
#define K230_DW_SSI_XIP_MODE_BITS_WRITABLE_MASK \
    R_XIP_MODE_BITS_XIP_MD_BITS_MASK
#define K230_DW_SSI_XIP_INCR_INST_WRITABLE_MASK \
    R_XIP_INCR_INST_INCR_INST_MASK
#define K230_DW_SSI_XIP_WRAP_INST_WRITABLE_MASK \
    R_XIP_WRAP_INST_WRAP_INST_MASK
#define K230_DW_SSI_SPIDR_WRITABLE_MASK R_SPIDR_SPI_INST_MASK
#define K230_DW_SSI_SPIAR_WRITABLE_MASK R_SPIAR_SDAR_MASK
#define K230_DW_SSI_AXIAR0_WRITABLE_MASK R_AXIAR0_AXIAR_0_31_MASK
#define K230_DW_SSI_AXIAR1_WRITABLE_MASK R_AXIAR1_AXIAR_32_63_MASK

/*
 * 普通保存寄存器直接从 regs[] 读回，所以复位值和所有 Guest 写入路径
 * 都必须通过各自的 writable mask，保证保留位不会进入寄存器状态。
 */
static void k230_dw_ssi_write_masked(K230DwSsiState *s, unsigned int reg,
                                     uint32_t value, uint32_t mask)
{
    s->regs[reg] = (s->regs[reg] & ~mask) | (value & mask);
}

static uint32_t k230_dw_ssi_frame_masked(K230DwSsiState *s)
{
    /*
     * LEARNING(P3): CTRLR0.DFS 保存的是“实际位数减一”。例如 DFS=7
     * 表示 8-bit frame。FIFO 的每一项仍是 uint32_t，但只有低 bits 位
     * 属于当前 SPI 帧，其余高位不能泄漏到线路或 RX FIFO。
     */
    unsigned int bits = FIELD_EX32(s->regs[R_CTRLR0], CTRLR0, DFS) + 1;

    return bits == 32 ? UINT32_MAX : MAKE_64BIT_MASK(0, bits);
}


static bool k230_dw_ssi_enabled(K230DwSsiState *s)
{
    /*
     * LEARNING(P3): SSIENR 只决定传输引擎能否运行，不应自动等价为
     * “DR 不可访问”。TX FIFO 预填和线路发送是两个不同动作。
     */
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

    /* 当前 profile 不支持广播片选，不能静默选择最低有效位。 */
    if (ser & (ser - 1)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: multiple chip selects enabled: 0x%x\n",
                      DEVICE(s)->canonical_path, ser);
        k230_dw_ssi_deselect(s);
        return;
    }

    k230_dw_ssi_select(s, ctz32(ser));
}

static void k230_dw_ssi_abort_transfer(K230DwSsiState *s)
{
    /*
     * 当前 ssi_transfer() 在一次 MMIO 回调内同步完成；禁用控制器时，
     * 通过拉高 CS 结束外设命令，并丢弃尚未消费的 FIFO 数据。
     */
    k230_dw_ssi_deselect(s);
    fifo32_reset(&s->tx_fifo);
    fifo32_reset(&s->rx_fifo);

    /*
     * LEARNING(P3): abort 必须同时清理 FIFO 和跨调用状态，否则下次
     * enable 可能从旧的 RO/EEPROM 阶段继续，形成幽灵事务。
     */
    s->phase = K230_DW_SSI_PHASE_IDLE;
    s->remaining_frames = 0;
    memset(&s->enhanced, 0, sizeof(s->enhanced));
}

static uint32_t k230_dw_ssi_status(K230DwSsiState *s)
{
    uint32_t tx_used = fifo32_num_used(&s->tx_fifo);
    uint32_t rx_used = fifo32_num_used(&s->rx_fifo);
    uint32_t sr = 0;

    /*
     * LEARNING(P3): SR 是动态视图，不需要依赖保存的 regs[R_SR]。
     * FIFO 水位变化后，下次读取 SR 就应立即看到新状态。
     */
    sr = FIELD_DP32(sr, SR, BUSY, s->phase != K230_DW_SSI_PHASE_IDLE);
    sr = FIELD_DP32(sr, SR, TFNF, tx_used < K230_DW_SSI_FIFO_CAPACITY);
    sr = FIELD_DP32(sr, SR, TFE, tx_used == 0);
    sr = FIELD_DP32(sr, SR, RFNE, rx_used != 0);
    sr = FIELD_DP32(sr, SR, RFF,
                    rx_used == K230_DW_SSI_FIFO_CAPACITY);

    return sr;
}

static void k230_dw_ssi_run_transfer(K230DwSsiState *s);

static void k230_dw_ssi_push_tx(K230DwSsiState *s, uint32_t tx)
{
    /*
     * LEARNING(P3-1): SSIENR=0 时忽略 DR 写；SSIENR=1、SER=0 时
     * 允许预填 TX FIFO。/pio/dr-aliases 与 /pio/dr-disabled-ignored
     * 分别覆盖这两个边界。
     */
    if (!k230_dw_ssi_enabled(s)) {
        return;
    }

    if (fifo32_is_full(&s->tx_fifo)) {
        /*
         * LEARNING(P3): Patch 3 只保证 FIFO 不越界。TXO 错误锁存属于
         * IRQ Patch，因此这里暂时直接忽略超出容量的写入。
         */
        return;
    }

    /*
     * LEARNING(P5): FIFO 必须保留 Guest 写入的完整 DR 项。Standard SPI
     * 仍由 send_frame() 按 DFS 截断；Enhanced SPI 则可能在 DFS=8 时把
     * 一个完整的 24/32-bit 地址放在单个 DR 项中，再按 ADDR_L 拆分。
     */
    fifo32_push(&s->tx_fifo, tx);

    /*
     * LEARNING(P3): push 只表示“数据已准备好”；pump 会再次检查
     * SSIENR、CS 和传输格式，决定现在发送还是继续留在 FIFO 中。
     */
    k230_dw_ssi_run_transfer(s);
}

/*
 * LEARNING(P3-2): 交换一个 Standard SPI 帧的 helper。
 *
 * 这个 helper 只负责：
 *   1. 按 DFS 截断发送帧；
 *   2. SRL=1 时执行内部回环；
 *   3. 否则调用 ssi_transfer()；
 *   4. 按 DFS 截断接收帧。
 *
 * 它不负责 FIFO、TMOD、CS、BUSY 或 IRQ。
 */
static uint32_t k230_dw_ssi_send_frame(K230DwSsiState *s,
                                        uint32_t tx)
{
    uint32_t mask = k230_dw_ssi_frame_masked(s);
    uint32_t rx;

    tx &= mask;

    if (FIELD_EX32(s->regs[R_CTRLR0], CTRLR0, SRL)) {
        rx = tx;                         /* 内部回环 */
    } else {
        rx = ssi_transfer(s->spi, tx);   /* 交给 SPI 总线 */
    }

    return rx & mask;
}

static bool k230_dw_ssi_enhanced_config_supported(K230DwSsiState *s)
{
    /*
     * TODO(P5-1): 从 CTRLR0.SPI_FRF、SPI_CTRLR0 和 max_lines 判断当前
     * 增强配置是否可执行。
     *
     * 本 Patch 只接受 Dual/Quad SDR，并要求 TRANS_TYPE 为 0、1 或 2。
     * 必须拒绝 Octal、超过实例 max_lines、TRANS_TYPE=3、SPI_DDR_EN、
     * INST_DDR_EN、SPI_RXDS_EN 和 SPI_RXDS_SIG_EN。拒绝时不要消费 FIFO、
     * 不要进入 BUSY，也不要伪造 RX 数据。
     */
    uint32_t ctrlr0 = s->regs[R_CTRLR0];
    uint32_t spi_ctrlr0 = s->regs[R_SPI_CTRLR0];
    uint32_t spi_frf;
    uint32_t trans_type;
    uint32_t required_lines;

    spi_frf = FIELD_EX32(ctrlr0, CTRLR0, SPI_FRF);
    trans_type = FIELD_EX32(spi_ctrlr0, SPI_CTRLR0, TRANS_TYPE);

    switch (spi_frf) {
    case 1: /* Dual */
        required_lines = 2;
        break;
    case 2: /* Quad */
        required_lines = 4;
        break;
    default:
        /* Standard 不会走到这里，Octal 明确不支持 */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unsupported SPI_FRF=%u\n",
                      DEVICE(s)->canonical_path, spi_frf);
        return false;
    }

    if (required_lines > s->max_lines) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: SPI_FRF=%u requires %u lines, only %u available\n",
                      DEVICE(s)->canonical_path,
                      spi_frf, required_lines, s->max_lines);
        return false;
    }

    if (trans_type > 2) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unsupported TRANS_TYPE=%u\n",
                      DEVICE(s)->canonical_path, trans_type);
        return false;
    }

    if (FIELD_EX32(spi_ctrlr0, SPI_CTRLR0, SPI_DDR_EN) ||
        FIELD_EX32(spi_ctrlr0, SPI_CTRLR0, INST_DDR_EN) ||
        FIELD_EX32(spi_ctrlr0, SPI_CTRLR0, SPI_RXDS_EN) ||
        FIELD_EX32(spi_ctrlr0, SPI_CTRLR0, SPI_RXDS_SIG_EN)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: DDR/RXDS enhanced mode is unsupported\n",
                      DEVICE(s)->canonical_path);
        return false;
    }

    return true;
}

static bool k230_dw_ssi_prepare_enhanced_command(K230DwSsiState *s)
{
    /*
     * TODO(P5-2): 根据 CTRLR0/SPI_CTRLR0/CTRLR1 和 TX FIFO 构造
     * s->enhanced，并在成功后把 phase 置为 ENHANCED_INSTRUCTION。
     *
     * 关键编码：INST_L=0/4/8/16 bit；ADDR_L 以 4 bit 为单位；实际数据
     * 帧数为 NDF+1。RO 的 TX FIFO 中，SDK PIO 路径依次写 instruction
     * 和 address；它们是两个“逻辑字段”，不能按 DFS=8 提前截断地址。
     * XIP_MD_BIT_EN=1 时还要从 XIP_MODE_BITS 取得 mode_bits，并把
     * XIP_MBL=0/1/2/3 解码为 2/4/8/16 bit。
     *
     * FIFO 项不足时直接返回 false，且不要只 pop 一半命令。
     */
    uint32_t spi_frf = FIELD_EX32(s->regs[R_CTRLR0], CTRLR0, SPI_FRF);
    uint32_t inst_l = FIELD_EX32(s->regs[R_SPI_CTRLR0], SPI_CTRLR0, INST_L);
    uint32_t addr_l = FIELD_EX32(s->regs[R_SPI_CTRLR0], SPI_CTRLR0, ADDR_L);
    uint32_t trans_type = FIELD_EX32(s->regs[R_SPI_CTRLR0],
                                     SPI_CTRLR0, TRANS_TYPE);
    uint32_t inst_bits;
    uint32_t addr_bits = addr_l << 2;
    uint32_t mode_bits = 0;
    uint32_t required_items;
    bool mode_bits_enabled =
        FIELD_EX32(s->regs[R_SPI_CTRLR0], SPI_CTRLR0, XIP_MD_BIT_EN);
    K230DwSsiEnhancedCommand command = { 0 };

    /* INST_L 是编码值：0/1/2/3 分别代表 0/4/8/16 bit。 */
    inst_bits = inst_l ? (1U << (inst_l + 1)) : 0;

    /* 当前 descriptor 和单个 DR 项最多保存 32-bit 地址。 */
    if (addr_bits > 32) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unsupported enhanced address length %u bits\n",
                      DEVICE(s)->canonical_path, addr_bits);
        return false;
    }

    if (mode_bits_enabled) {
        uint32_t mode_length_encoding =
            FIELD_EX32(s->regs[R_SPI_CTRLR0], SPI_CTRLR0, XIP_MBL);

        mode_bits = 1U << (mode_length_encoding + 1);
    }

    /* instruction/address 是两个逻辑字段，各占一个 FIFO 项。 */
    required_items = (inst_bits != 0) + (addr_bits != 0);

    /*
     * 数据不足时保持 FIFO、phase 和 descriptor 不变。
     * 下一次 DR 写入或 SER 选择触发 pump 后会重新尝试。
     */
    if (fifo32_num_used(&s->tx_fifo) < required_items) {
        return false;
    }

    command.instruction_bits = inst_bits;
    command.address_bits = addr_bits;
    command.mode_bits = mode_bits;
    command.mode_bits_enabled = mode_bits_enabled;
    command.wait_cycles =
        FIELD_EX32(s->regs[R_SPI_CTRLR0], SPI_CTRLR0, WAIT_CYCLES);
    command.data_frames =
        FIELD_EX32(s->regs[R_CTRLR1], CTRLR1, NDF) + 1;
    command.spi_frf = spi_frf;
    command.trans_type = trans_type;

    if (inst_bits != 0) {
        command.instruction = fifo32_pop(&s->tx_fifo) &
            (uint32_t)MAKE_64BIT_MASK(0, inst_bits);
    }
    if (addr_bits != 0) {
        command.address = fifo32_pop(&s->tx_fifo) &
            (uint32_t)MAKE_64BIT_MASK(0, addr_bits);
    }
    if (mode_bits_enabled) {
        command.mode = s->regs[R_XIP_MODE_BITS] &
            (uint32_t)MAKE_64BIT_MASK(0, mode_bits);
    }

    s->enhanced = command;
    s->remaining_frames = command.data_frames;
    s->phase = K230_DW_SSI_PHASE_ENHANCED_INSTRUCTION;
    return true;
}

static void k230_dw_ssi_send_enhanced_field(K230DwSsiState *s,
                                             uint32_t value,
                                             uint32_t bits)
{
    uint32_t bytes = DIV_ROUND_UP(bits, 8);

    /* SSIBus/m25p80 是字节级模型，增强线宽只保存在 command 描述中。 */
    for (uint32_t i = 0; i < bytes; i++) {
        uint32_t shift = (bytes - i - 1) * 8;

        ssi_transfer(s->spi, (value >> shift) & 0xff);
    }
}

static void k230_dw_ssi_run_enhanced_transfer(K230DwSsiState *s)
{
    /*
     * LEARNING(P5-3): 按以下顺序推进同一条增强事务：
     *
     *   instruction -> address -> mode -> dummy -> data
     *
     * instruction/address 要按各自位数以大端总线字节发送；没有启用
     * mode 时跳过该阶段。QEMU m25p80 是字节级模型，dummy 的换算必须
     * 与其“dummy cycle modeled with byte writes”契约对齐，不能硬编码
     * 0x6b/0xeb 等 opcode。数据阶段使用 remaining_frames=NDF+1，发送
     * dummy 并把返回值压入 RX FIFO；RX FIFO 满时保留 phase 和计数，
     * 等 Guest 读取 DR 后由同一个 pump 恢复。完成后回到 IDLE。
     */
    if (s->phase == K230_DW_SSI_PHASE_IDLE) {
        if (!k230_dw_ssi_enhanced_config_supported(s) ||
            !k230_dw_ssi_prepare_enhanced_command(s)) {
            return;
        }
    }

    if (s->phase == K230_DW_SSI_PHASE_ENHANCED_INSTRUCTION) {
        if (s->enhanced.instruction_bits != 0) {
            k230_dw_ssi_send_enhanced_field(
                s, s->enhanced.instruction,
                s->enhanced.instruction_bits);
        }
        s->phase = K230_DW_SSI_PHASE_ENHANCED_ADDRESS;
    }

    if (s->phase == K230_DW_SSI_PHASE_ENHANCED_ADDRESS) {
        if (s->enhanced.address_bits != 0) {
            k230_dw_ssi_send_enhanced_field(
                s, s->enhanced.address, s->enhanced.address_bits);
        }
        s->phase = K230_DW_SSI_PHASE_ENHANCED_MODE;
    }

    if (s->phase == K230_DW_SSI_PHASE_ENHANCED_MODE) {
        if (s->enhanced.mode_bits_enabled) {
            k230_dw_ssi_send_enhanced_field(
                s, s->enhanced.mode, s->enhanced.mode_bits);
        }
        s->phase = K230_DW_SSI_PHASE_ENHANCED_DUMMY;
    }

    if (s->phase == K230_DW_SSI_PHASE_ENHANCED_DUMMY) {
        for (uint32_t i = 0; i < s->enhanced.wait_cycles; i++) {
            ssi_transfer(s->spi, 0);
        }
        s->phase = K230_DW_SSI_PHASE_ENHANCED_DATA;
    }

    if (s->phase != K230_DW_SSI_PHASE_ENHANCED_DATA) {
        g_assert_not_reached();
    }

    while (!fifo32_is_full(&s->rx_fifo) &&
           s->remaining_frames > 0) {
        uint32_t rx = ssi_transfer(s->spi, 0);

        fifo32_push(&s->rx_fifo,
                    rx & k230_dw_ssi_frame_masked(s));
        s->remaining_frames--;
    }

    if (s->remaining_frames == 0) {
        s->phase = K230_DW_SSI_PHASE_IDLE;
    }
}

static void k230_dw_ssi_run_transfer(K230DwSsiState *s)
{
    uint32_t spi_frf;
    uint32_t tmod;

    if (!k230_dw_ssi_enabled(s) || s->active_cs < 0) {
        return;
    }

    spi_frf = FIELD_EX32(s->regs[R_CTRLR0], CTRLR0, SPI_FRF);
    if (spi_frf != 0) {
        k230_dw_ssi_run_enhanced_transfer(s);
        return;
    }

    tmod = FIELD_EX32(s->regs[R_CTRLR0], CTRLR0, TMOD);

    switch (tmod) {
    case 0: /* TX_AND_RX */
        /*
         * LEARNING(P3-3): 消费 TX FIFO，并把每帧返回值压入 RX FIFO。
         *
         * 复盘问题：
         *   - 循环条件观察哪个 FIFO？
         *   - tx 在什么时候 pop？
         *   - RX FIFO 满时能否继续 push？
         *   - 一次 TX 帧最多对应几个 RX FIFO 项？
         */
        while (!fifo32_is_empty(&s->tx_fifo)) {
            uint32_t tx = fifo32_pop(&s->tx_fifo);
            uint32_t rx = k230_dw_ssi_send_frame(s, tx);
            if(!fifo32_is_full(&s->rx_fifo)) {
                fifo32_push(&s->rx_fifo, rx);
            }
        }
        break;
    case 1: /* TX_ONLY */
        /*
         * LEARNING(P3-4): 消费 TX FIFO，但丢弃线路返回值。
         *
         * LEARNING(P3): SPI 线路本身仍会同时返回数据；TX_ONLY 的含义
         * 是控制器不把返回值写入 RX FIFO，而不是“线路没有 RX”。
         */
        while (!fifo32_is_empty(&s->tx_fifo)) {
            uint32_t tx = fifo32_pop(&s->tx_fifo);
            k230_dw_ssi_send_frame(s, tx);
        }
        break;
    case 2: /* RX_ONLY */
        /*
         * LEARNING(P3-5): 一个 dummy DR 启动 NDF + 1 个接收帧。
         *
         * 建议分成两个小步骤：
         *   1. IDLE 时检查并消费一个 dummy TX FIFO 项，初始化状态；
         *   2. RX_ONLY 阶段按 remaining_frames 生成接收帧。
         *
         * RX FIFO 满时要保留 phase/remaining_frames，不能忙等。
         */
        switch (s->phase) {
            case K230_DW_SSI_PHASE_IDLE:
                /* 没有 dummy DR，无法启动 RX_ONLY */
                if (fifo32_is_empty(&s->tx_fifo)) {
                    break;
                }
                /* 消费 dummy DR，启动 RX_ONLY */
                fifo32_pop(&s->tx_fifo);
                s->phase = K230_DW_SSI_PHASE_RX_ONLY;
                s->remaining_frames = FIELD_EX32(s->regs[R_CTRLR1], CTRLR1, NDF) + 1;

            case K230_DW_SSI_PHASE_RX_ONLY:
                /* 处理接收帧 */
                while (!fifo32_is_full(&s->rx_fifo) && s->remaining_frames > 0) {
                    uint32_t rx = k230_dw_ssi_send_frame(s, 0x00);
                    fifo32_push(&s->rx_fifo, rx);
                    s->remaining_frames--;
                }
                if (s->remaining_frames == 0) {
                    s->phase = K230_DW_SSI_PHASE_IDLE;
                }
                break;
        }
        break;
    case 3: /* EEPROM_READ */
        /*
         * LEARNING(P3-6): 先发送命令阶段，再自动接收 NDF + 1 帧。
         *
         * COMMAND 阶段：消费 TX FIFO，线路返回值无效。
         * DATA 阶段：不再消费命令帧，使用 dummy 产生接收时钟。
         * 两个阶段不能共用 TR 的“发送一次就保存一次 RX”逻辑。
         */
        switch (s->phase) {
            case K230_DW_SSI_PHASE_IDLE:
                if(!fifo32_is_empty(&s->tx_fifo)) {
                    s->phase = K230_DW_SSI_PHASE_EEPROM_COMMAND;
                }
                else
                    break;
            case K230_DW_SSI_PHASE_EEPROM_COMMAND:
                /* 没有命令帧，无法启动 EEPROM_READ */
                if (fifo32_is_empty(&s->tx_fifo))
                {
                    break;
                }
                while (!fifo32_is_empty(&s->tx_fifo)) {
                    /* 消费命令帧，启动 EEPROM_READ */
                    uint32_t tx = fifo32_pop(&s->tx_fifo);
                    k230_dw_ssi_send_frame(s, tx);
                }
                s->phase = K230_DW_SSI_PHASE_EEPROM_DATA;
                s->remaining_frames = FIELD_EX32(s->regs[R_CTRLR1], CTRLR1, NDF) + 1;

            case K230_DW_SSI_PHASE_EEPROM_DATA:
                /* 处理接收帧 */
                while (!fifo32_is_full(&s->rx_fifo) && s->remaining_frames > 0) {
                    uint32_t rx = k230_dw_ssi_send_frame(s, 0x00);
                    fifo32_push(&s->rx_fifo, rx);
                    s->remaining_frames--;
                }
                if (s->remaining_frames == 0) {
                    s->phase = K230_DW_SSI_PHASE_IDLE;
                }
                break;
        }
        break;
    default:
        g_assert_not_reached();
    }
}



static bool k230_dw_ssi_is_dr(hwaddr addr)
{
    return addr >= A_DR0 && addr <= A_DR_END &&
           (addr & 0x3) == 0;
}

static bool k230_dw_ssi_is_razwi(hwaddr addr)
{
    /* 这些条件寄存器未集成到当前 K230 profile，统一按 RAZ/WI 处理。 */
    switch (addr) {
    case A_XIP_CTRL:
    case A_XIP_SER:
    case A_XRXOICR:
    case A_XIP_CNT_TIME_OUT:
    case A_SPI_CTRLR1:
    case A_SPITECR:
    case A_RSVD_138:
    case A_RSVD_13C:
    case A_XIP_WRITE_INCR_INST:
    case A_XIP_WRITE_WRAP_INST:
    case A_XIP_WRITE_CTRL:
        return true;
    default:
        return false;
    }
}

static bool k230_dw_ssi_write_requires_disabled(hwaddr addr)
{
    /*
     * 仅锁定会直接改变基本事务格式的静态寄存器。SSIENR=1 只表示
     * 控制器已启用，不等于 SPI、IDMA 或 XIP 事务正在进行；对于 TRM
     * 未明确规定为 disabled-only 的 DMA、XIP 和采样时序寄存器，接受
     * 软件写入，由 guest 驱动负责在安全的事务边界更新。
     */
    switch (addr) {
    case A_CTRLR0:
    case A_CTRLR1:
    case A_MWCR:
    case A_BAUDR:
    case A_SPI_CTRLR0:
        return true;
    default:
        return false;
    }
}

static uint64_t k230_dw_ssi_read(void *opaque, hwaddr addr, unsigned int size)
{
    K230DwSsiState *s = K230_DW_SSI(opaque);
    uint32_t value = 0;

    if (k230_dw_ssi_is_dr(addr)) {
        if (!fifo32_is_empty(&s->rx_fifo)) {
            /* LEARNING(P3): DR0..DR35 都是同一个 RX FIFO 的别名。 */
            value = fifo32_pop(&s->rx_fifo) & k230_dw_ssi_frame_masked(s);
        }

        /*
         * LEARNING(P3-7): RX FIFO 腾出空间后恢复未完成的接收；观察
         * phase、remaining_frames 与 RX FIFO 是否仍然满。
         */
        k230_dw_ssi_run_transfer(s);
        return value;
    }

    if (k230_dw_ssi_is_razwi(addr)) {
        return 0;
    }

    switch (addr) {
    /* 这些寄存器的复位和写入路径已完成掩码，可直接返回保存值。 */
    case A_CTRLR0:
    case A_CTRLR1:
    case A_SSIENR:
    case A_MWCR:
    case A_BAUDR:
    case A_TXFTLR:
    case A_RXFTLR:
    case A_IMR:
    case A_DMACR:
    case A_AXIAWLEN:
    case A_AXIARLEN:
    case A_IDR:
    case A_SSIC_VERSION_ID:
    case A_RX_SAMPLE_DELAY:
    case A_SPI_CTRLR0:
    case A_DDR_DRIVE_EDGE:
    case A_XIP_MODE_BITS:
    case A_XIP_INCR_INST:
    case A_XIP_WRAP_INST:
    case A_SPIDR:
    case A_SPIAR:
    case A_AXIAR0:
    case A_AXIAR1:
        value = s->regs[addr / sizeof(uint32_t)];
        break;
    case A_SER:
        value = s->regs[R_SER] & MAKE_64BIT_MASK(0, s->num_cs);
        break;
    case A_TXFLR:
        value = fifo32_num_used(&s->tx_fifo);
        break;
    case A_RXFLR:
        value = fifo32_num_used(&s->rx_fifo);
        break;
    case A_SR:
        value = k230_dw_ssi_status(s);
        break;
    case A_ISR:
    case A_RISR:
        value = 0;
        break;
    case A_TXEICR:
    case A_RXOICR:
    case A_RXUICR:
    case A_MSTICR:
    case A_ICR:
        value = 0;
        break;
    case A_AXIECR:
    case A_DONECR:
        value = 0;
        break;
    default:
        if (addr >= K230_DW_SSI_REGS_SIZE || (addr & 0x3) != 0) {
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
        k230_dw_ssi_push_tx(s, value);
        return;
    }

    if (k230_dw_ssi_is_razwi(addr)) {
        return;
    }

    if (k230_dw_ssi_write_requires_disabled(addr) &&
        k230_dw_ssi_enabled(s)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to offset 0x%" HWADDR_PRIx
                      " while SSI is enabled\n",
                      DEVICE(s)->canonical_path, addr);
        return;
    }

    switch (addr) {
    case A_CTRLR0:
        k230_dw_ssi_write_masked(s, R_CTRLR0, value,
                                 K230_DW_SSI_CTRLR0_WRITABLE_MASK);
        break;
    case A_CTRLR1:
        k230_dw_ssi_write_masked(s, R_CTRLR1, value,
                                 K230_DW_SSI_CTRLR1_WRITABLE_MASK);
        break;
    case A_SSIENR: {
        bool old_enabled = k230_dw_ssi_enabled(s);
        bool new_enabled = value & R_SSIENR_SSIC_EN_MASK;

        /* 只有 0→1 和 1→0 状态转换产生副作用，重复写同值直接忽略。 */
        if (old_enabled == new_enabled) {
            return;
        }

        s->regs[R_SSIENR] = FIELD_DP32(0, SSIENR, SSIC_EN, new_enabled);
        if (!new_enabled) {
            k230_dw_ssi_abort_transfer(s);
            return;
        }

        k230_dw_ssi_update_cs(s);
        /*
         * LEARNING(P3): enable 不只是改变寄存器位，还可能让之前预填
         * 的 TX FIFO 首次具备发送条件，因此需要主动运行 pump。
         */
        k230_dw_ssi_run_transfer(s);
        break;
    }
    case A_MWCR:
        k230_dw_ssi_write_masked(s, R_MWCR, value,
                                 K230_DW_SSI_MWCR_WRITABLE_MASK);
        break;
    case A_SER:
        /* SER 选择后，已预填的 TX FIFO 首次具备线路发送条件。 */
        s->regs[R_SER] = value & MAKE_64BIT_MASK(0, s->num_cs);
        k230_dw_ssi_update_cs(s);
        k230_dw_ssi_run_transfer(s);
        break;
    case A_BAUDR:
        k230_dw_ssi_write_masked(s, R_BAUDR, value,
                                 K230_DW_SSI_BAUDR_WRITABLE_MASK);
        break;
    case A_TXFTLR:
        k230_dw_ssi_write_masked(s, R_TXFTLR, value,
                                 K230_DW_SSI_TXFTLR_WRITABLE_MASK);
        break;
    case A_RXFTLR:
        k230_dw_ssi_write_masked(s, R_RXFTLR, value,
                                 K230_DW_SSI_RXFTLR_WRITABLE_MASK);
        break;
    case A_TXFLR:
    case A_RXFLR:
    case A_SR:
        break;
    case A_IMR:
        k230_dw_ssi_write_masked(s, R_IMR, value,
                                 K230_DW_SSI_IMR_WRITABLE_MASK);
        break;
    case A_ISR:
    case A_RISR:
        break;
    case A_TXEICR:
    case A_RXOICR:
    case A_RXUICR:
    case A_MSTICR:
    case A_ICR:
        break;
    case A_DMACR:
        k230_dw_ssi_write_masked(s, R_DMACR, value,
                                 K230_DW_SSI_DMACR_WRITABLE_MASK);
        if (FIELD_EX32(s->regs[R_DMACR], DMACR, IDMAE)) {
            qemu_log_mask(LOG_UNIMP,
                          "%s: DMACR.IDMAE enabled, internal DMA is not "
                          "implemented\n",
                          DEVICE(s)->canonical_path);
        }
        break;
    case A_AXIAWLEN:
        k230_dw_ssi_write_masked(s, R_AXIAWLEN, value,
                                 K230_DW_SSI_AXIAWLEN_WRITABLE_MASK);
        break;
    case A_AXIARLEN:
        k230_dw_ssi_write_masked(s, R_AXIARLEN, value,
                                 K230_DW_SSI_AXIARLEN_WRITABLE_MASK);
        break;
    case A_IDR:
    case A_SSIC_VERSION_ID:
        break;
    case A_RX_SAMPLE_DELAY:
        k230_dw_ssi_write_masked(
            s, R_RX_SAMPLE_DELAY, value,
            K230_DW_SSI_RX_SAMPLE_DELAY_WRITABLE_MASK);
        break;
    case A_SPI_CTRLR0:
        k230_dw_ssi_write_masked(
            s, R_SPI_CTRLR0, value,
            K230_DW_SSI_SPI_CTRLR0_WRITABLE_MASK);
        break;
    case A_DDR_DRIVE_EDGE:
        k230_dw_ssi_write_masked(
            s, R_DDR_DRIVE_EDGE, value,
            K230_DW_SSI_DDR_DRIVE_EDGE_WRITABLE_MASK);
        break;
    case A_XIP_MODE_BITS:
        k230_dw_ssi_write_masked(
            s, R_XIP_MODE_BITS, value,
            K230_DW_SSI_XIP_MODE_BITS_WRITABLE_MASK);
        break;
    case A_XIP_INCR_INST:
        k230_dw_ssi_write_masked(
            s, R_XIP_INCR_INST, value,
            K230_DW_SSI_XIP_INCR_INST_WRITABLE_MASK);
        break;
    case A_XIP_WRAP_INST:
        k230_dw_ssi_write_masked(
            s, R_XIP_WRAP_INST, value,
            K230_DW_SSI_XIP_WRAP_INST_WRITABLE_MASK);
        break;
    case A_SPIDR:
        k230_dw_ssi_write_masked(s, R_SPIDR, value,
                                 K230_DW_SSI_SPIDR_WRITABLE_MASK);
        break;
    case A_SPIAR:
        k230_dw_ssi_write_masked(s, R_SPIAR, value,
                                 K230_DW_SSI_SPIAR_WRITABLE_MASK);
        break;
    case A_AXIAR0:
        k230_dw_ssi_write_masked(s, R_AXIAR0, value,
                                 K230_DW_SSI_AXIAR0_WRITABLE_MASK);
        break;
    case A_AXIAR1:
        k230_dw_ssi_write_masked(s, R_AXIAR1, value,
                                 K230_DW_SSI_AXIAR1_WRITABLE_MASK);
        break;
    case A_AXIECR:
    case A_DONECR:
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
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void k230_dw_ssi_enter_reset(Object *obj, ResetType type)
{
    K230DwSsiState *s = K230_DW_SSI(obj);

    memset(s->regs, 0, sizeof(s->regs));
    fifo32_reset(&s->tx_fifo);
    fifo32_reset(&s->rx_fifo);
    s->phase = K230_DW_SSI_PHASE_IDLE;
    s->remaining_frames = 0;
    memset(&s->enhanced, 0, sizeof(s->enhanced));

    s->regs[R_CTRLR0] = K230_DW_SSI_CTRLR0_RESET;
    s->regs[R_SR] = K230_DW_SSI_SR_RESET;
    s->regs[R_IMR] = K230_DW_SSI_IMR_RESET;
    s->regs[R_AXIAWLEN] = K230_DW_SSI_AXIAWLEN_RESET;
    s->regs[R_AXIARLEN] = K230_DW_SSI_AXIARLEN_RESET;
    s->regs[R_IDR] = K230_DW_SSI_IDR_RESET;
    s->regs[R_SSIC_VERSION_ID] = K230_DW_SSI_VERSION;
    s->regs[R_SPI_CTRLR0] = s->max_lines == 8 ?
        K230_DW_SSI_SPI_CTRLR0_FMC_RESET :
        K230_DW_SSI_SPI_CTRLR0_SPI_RESET;
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
}

static const VMStateDescription vmstate_k230_dw_ssi = {
    .name = TYPE_K230_DW_SSI,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, K230DwSsiState, K230_DW_SSI_NUM_REGS),
        VMSTATE_FIFO32(tx_fifo, K230DwSsiState),
        VMSTATE_FIFO32(rx_fifo, K230DwSsiState),
        VMSTATE_UINT32(phase, K230DwSsiState),
        VMSTATE_UINT32(remaining_frames, K230DwSsiState),
        VMSTATE_UINT32(enhanced.instruction, K230DwSsiState),
        VMSTATE_UINT32(enhanced.address, K230DwSsiState),
        VMSTATE_UINT32(enhanced.mode, K230DwSsiState),
        VMSTATE_UINT32(enhanced.mode_bits, K230DwSsiState),
        VMSTATE_UINT32(enhanced.instruction_bits, K230DwSsiState),
        VMSTATE_UINT32(enhanced.address_bits, K230DwSsiState),
        VMSTATE_UINT32(enhanced.wait_cycles, K230DwSsiState),
        VMSTATE_UINT32(enhanced.data_frames, K230DwSsiState),
        VMSTATE_UINT32(enhanced.spi_frf, K230DwSsiState),
        VMSTATE_UINT32(enhanced.trans_type, K230DwSsiState),
        VMSTATE_BOOL(enhanced.mode_bits_enabled, K230DwSsiState),
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

    memory_region_init_io(&s->mmio, obj, &k230_dw_ssi_ops, s,
                          TYPE_K230_DW_SSI, K230_DW_SSI_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);

    fifo32_create(&s->tx_fifo, K230_DW_SSI_FIFO_CAPACITY);
    fifo32_create(&s->rx_fifo, K230_DW_SSI_FIFO_CAPACITY);
    s->active_cs = -1;
}

static void k230_dw_ssi_realize(DeviceState *dev, Error **errp)
{
    K230DwSsiState *s = K230_DW_SSI(dev);

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

    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    qdev_init_gpio_out_named(dev, s->cs_lines, "cs", s->num_cs);
}

static void k230_dw_ssi_finalize(Object *obj)
{
    K230DwSsiState *s = K230_DW_SSI(obj);

    fifo32_destroy(&s->tx_fifo);
    fifo32_destroy(&s->rx_fifo);
    g_free(s->cs_lines);
}

static const Property k230_dw_ssi_properties[] = {
    DEFINE_PROP_UINT32("num-cs", K230DwSsiState, num_cs, 1),
    DEFINE_PROP_UINT32("max-lines", K230DwSsiState, max_lines, 1),
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
