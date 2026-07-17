/*
 * K230 DWC SSI qtest 共享契约
 *
 * 常量以 K230 TRM 12.3/5.3、K230 SDK 的 dw_spi_reg_t、
 * designware_spi.c 和 k230.dtsi 为准，不能从当前设备模型反推。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef K230_DW_SSI_TEST_H
#define K230_DW_SSI_TEST_H

#include "qemu/bitops.h"
#include "qemu/units.h"
#include "libqtest.h"

#define K230_SPI0_BASE          0x91584000ULL
#define K230_SPI1_BASE          0x91582000ULL
#define K230_SPI2_BASE          0x91583000ULL
#define K230_HI_SYS_BASE        0x91585000ULL
#define K230_SSI_CTRL_ADDR      (K230_HI_SYS_BASE + 0x68)
#define K230_FLASH_BASE         0xc0000000ULL
#define K230_FLASH_SIZE         (128 * MiB)
#define K230_PLIC_BASE          0xf00000000ULL
#define K230_PLIC_PENDING_BASE  0x1000

#define K230_SSI_CTRLR0          0x000
#define K230_SSI_CTRLR1          0x004
#define K230_SSI_SSIENR          0x008
#define K230_SSI_MWCR            0x00c
#define K230_SSI_SER             0x010
#define K230_SSI_BAUDR           0x014
#define K230_SSI_TXFTLR          0x018
#define K230_SSI_RXFTLR          0x01c
#define K230_SSI_TXFLR           0x020
#define K230_SSI_RXFLR           0x024
#define K230_SSI_SR              0x028
#define K230_SSI_IMR             0x02c
#define K230_SSI_ISR             0x030
#define K230_SSI_RISR            0x034
#define K230_SSI_TXEICR          0x038
#define K230_SSI_RXOICR          0x03c
#define K230_SSI_RXUICR          0x040
#define K230_SSI_MSTICR          0x044
#define K230_SSI_ICR             0x048
#define K230_SSI_DMACR           0x04c
#define K230_SSI_AXIAWLEN        0x050
#define K230_SSI_AXIARLEN        0x054
#define K230_SSI_IDR             0x058
#define K230_SSI_VERSION_ID      0x05c
#define K230_SSI_DR0             0x060
#define K230_SSI_DR(n)           (K230_SSI_DR0 + (n) * 4)
#define K230_SSI_DR_COUNT        36
#define K230_SSI_RX_SAMPLE_DELAY 0x0f0
#define K230_SSI_SPI_CTRLR0      0x0f4
#define K230_SSI_DDR_DRIVE_EDGE  0x0f8
#define K230_SSI_XIP_MODE_BITS   0x0fc
#define K230_SSI_XIP_INCR_INST   0x100
#define K230_SSI_XIP_WRAP_INST   0x104
#define K230_SSI_XIP_CTRL        0x108
#define K230_SSI_XIP_SER         0x10c
#define K230_SSI_XRXOICR         0x110
#define K230_SSI_XIP_TIMEOUT     0x114
#define K230_SSI_SPI_CTRLR1      0x118
#define K230_SSI_SPITECR         0x11c
#define K230_SSI_SPIDR           0x120
#define K230_SSI_SPIAR           0x124
#define K230_SSI_AXIAR0          0x128
#define K230_SSI_AXIAR1          0x12c
#define K230_SSI_AXIECR          0x130
#define K230_SSI_DONECR          0x134
#define K230_SSI_RSVD_138        0x138
#define K230_SSI_RSVD_13C        0x13c
#define K230_SSI_XIP_WRITE_INCR  0x140
#define K230_SSI_XIP_WRITE_WRAP  0x144
#define K230_SSI_XIP_WRITE_CTRL  0x148

#define K230_SSI_CTRLR0_RESET           0x00004007U
#define K230_SSI_SPI_CTRLR0_SPI_RESET   0x04000200U
#define K230_SSI_SPI_CTRLR0_FMC_RESET   0x28000200U
#define K230_SSI_IMR_RESET              0x0000003fU
#define K230_SSI_AXILEN_RESET           0x00000700U
#define K230_SSI_IDR_RESET              0xa1b2c3d5U
#define K230_SSI_VERSION_RESET          0x3130332aU

#define K230_SSI_CTRLR0_WRITABLE_MASK   0x01cf7f1fU
#define K230_SSI_CTRLR1_WRITABLE_MASK   0x0000ffffU
#define K230_SSI_MWCR_WRITABLE_MASK     0x00000003U
#define K230_SSI_BAUDR_WRITABLE_MASK    0x0000fffeU
#define K230_SSI_TXFTLR_WRITABLE_MASK   0x07ff00ffU
#define K230_SSI_RXFTLR_WRITABLE_MASK   0x000000ffU
#define K230_SSI_IMR_WRITABLE_MASK      0x000009bfU
#define K230_SSI_DMACR_WRITABLE_MASK    0x0007ff5cU
#define K230_SSI_AXILEN_WRITABLE_MASK   0x0000ff00U
#define K230_SSI_RX_SAMPLE_WRITABLE_MASK 0x000100ffU
#define K230_SSI_SPI_CTRLR0_WRITABLE_MASK 0x6f3ffbbfU
#define K230_SSI_DDR_EDGE_WRITABLE_MASK 0x000000ffU
#define K230_SSI_XIP_REG_WRITABLE_MASK  0x0000ffffU
#define K230_SSI_SPIDR_WRITABLE_MASK    0x0000ffffU

#define K230_SSI_CTRL_RESET             0x00004000U
#define K230_SSI_CTRL_IMPLEMENTED_MASK  0x0003fff1U
#define K230_SSI_CTRL_WRITABLE_MASK     0x0003e001U
#define K230_SSI_CTRL_XIP_EN            BIT(0)
#define K230_SSI_CTRL_SPI0_SLEEP        BIT(4)
#define K230_SSI_CTRL_SPI0_MODE_SHIFT   5
#define K230_SSI_CTRL_SPI1_SLEEP        BIT(7)
#define K230_SSI_CTRL_SPI1_MODE_SHIFT   8
#define K230_SSI_CTRL_SPI2_SLEEP        BIT(10)
#define K230_SSI_CTRL_SPI2_MODE_SHIFT   11

#define K230_SSI_CTRLR0_DFS_MASK        0x1fU
#define K230_SSI_CTRLR0_TMOD_SHIFT      10
#define K230_SSI_CTRLR0_TMOD_MASK       (3U << K230_SSI_CTRLR0_TMOD_SHIFT)
#define K230_SSI_CTRLR0_SRL             BIT(13)
#define K230_SSI_CTRLR0_SPI_FRF_SHIFT   22
#define K230_SSI_CTRLR0_SPI_FRF_MASK    (3U << K230_SSI_CTRLR0_SPI_FRF_SHIFT)

#define K230_SSI_TMOD_TR                0
#define K230_SSI_TMOD_TO                1
#define K230_SSI_TMOD_RO                2
#define K230_SSI_TMOD_EEPROM_READ       3

#define K230_SSI_FRF_STANDARD           0
#define K230_SSI_FRF_DUAL               1
#define K230_SSI_FRF_QUAD               2
#define K230_SSI_FRF_OCTAL              3

#define K230_SSI_SPI_CTRLR0_TRANS_TYPE(v) ((v) & 0x3U)
#define K230_SSI_SPI_CTRLR0_ADDR_L(bits)  (((bits) / 4U) << 2)
#define K230_SSI_SPI_CTRLR0_XIP_MD_EN     BIT(7)
#define K230_SSI_SPI_CTRLR0_INST_L_8      (2U << 8)
#define K230_SSI_SPI_CTRLR0_WAIT(v)       (((v) & 0x1fU) << 11)
#define K230_SSI_SPI_CTRLR0_XIP_MBL_8     (2U << 26)
#define K230_SSI_SPI_CTRLR0_SPI_DDR_EN    BIT(16)
#define K230_SSI_SPI_CTRLR0_INST_DDR_EN   BIT(17)
#define K230_SSI_SPI_CTRLR0_RXDS_EN       BIT(18)
#define K230_SSI_SPI_CTRLR0_RXDS_SIG_EN   BIT(25)

#define K230_SSI_SR_BUSY                BIT(0)
#define K230_SSI_SR_TFNF                BIT(1)
#define K230_SSI_SR_TFE                 BIT(2)
#define K230_SSI_SR_RFNE                BIT(3)
#define K230_SSI_SR_RFF                 BIT(4)

#define K230_SSI_INT_TXE                BIT(0)
#define K230_SSI_INT_TXO                BIT(1)
#define K230_SSI_INT_RXU                BIT(2)
#define K230_SSI_INT_RXO                BIT(3)
#define K230_SSI_INT_RXF                BIT(4)
#define K230_SSI_INT_MST                BIT(5)
#define K230_SSI_INT_TXU                BIT(7)
#define K230_SSI_INT_AXIE               BIT(8)
#define K230_SSI_INT_DONE               BIT(11)

#define K230_SSI_IRQ_TXE                0
#define K230_SSI_IRQ_TXO                1
#define K230_SSI_IRQ_RXF                2
#define K230_SSI_IRQ_RXO                3
#define K230_SSI_IRQ_TXU                4
#define K230_SSI_IRQ_RXU                5
#define K230_SSI_IRQ_MST                6
#define K230_SSI_IRQ_DONE               7
#define K230_SSI_IRQ_AXIE               8
#define K230_SSI_IRQ_COUNT              9

#define K230_SSI_FIFO_DEPTH             256
#define K230_SSI_FLASH_IMAGE_SIZE       (32 * MiB)
#define K230_SSI_FLASH_PATTERN_ADDR     0x100
#define K230_SSI_FLASH_HIGH_ADDR        0x1000100
#define K230_SSI_FLASH_PROGRAM_ADDR     0x22000
#define K230_SSI_FLASH_ERASE_ADDR       0x24000

typedef struct K230SsiInstance {
    const char *name;
    uint64_t base;
    uint32_t num_cs;
    uint32_t max_lines;
    uint32_t spi_ctrlr0_reset;
    uint32_t first_irq;
    bool has_xip;
} K230SsiInstance;

typedef struct K230SsiFlashImage {
    char *path;
} K230SsiFlashImage;

extern const K230SsiInstance k230_ssi_instances[3];

QTestState *k230_ssi_start(void);
QTestState *k230_ssi_start_with_flash(K230SsiFlashImage *image);
void k230_ssi_flash_image_init(K230SsiFlashImage *image);
void k230_ssi_flash_image_clear(K230SsiFlashImage *image);

uint32_t k230_ssi_readl(QTestState *qts, uint64_t base, uint32_t offset);
void k230_ssi_writel(QTestState *qts, uint64_t base,
                     uint32_t offset, uint32_t value);
void k230_ssi_disable(QTestState *qts, uint64_t base);
void k230_ssi_configure(QTestState *qts, uint64_t base,
                        uint32_t tmod, uint32_t dfs_bits, uint32_t ndf);
void k230_ssi_enable_cs(QTestState *qts, uint64_t base, uint32_t ser);
void k230_ssi_write_frame(QTestState *qts, uint64_t base, uint32_t value);
uint32_t k230_ssi_read_frame(QTestState *qts, uint64_t base);
void k230_ssi_wait_mask(QTestState *qts, uint64_t base, uint32_t offset,
                        uint32_t mask, uint32_t expected);
void k230_ssi_drain_rx(QTestState *qts, uint64_t base);
bool k230_ssi_plic_pending(QTestState *qts, uint32_t irq);
uint32_t k230_ssi_frame_mask(uint32_t dfs_bits);
void k230_ssi_standard_transaction(QTestState *qts, uint64_t base,
                                   const uint8_t *tx, uint8_t *rx, size_t len);

void k230_ssi_register_reg_tests(void);
void k230_ssi_register_pio_tests(void);
void k230_ssi_register_irq_tests(void);
void k230_ssi_register_flash_tests(void);
void k230_ssi_register_qspi_tests(void);
void k230_ssi_register_xip_tests(void);

#endif
