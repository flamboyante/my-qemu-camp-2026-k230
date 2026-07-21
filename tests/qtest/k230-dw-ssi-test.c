/*
 * K230 DesignWare SSI controller qtest
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"
#include "qemu/units.h"

#define K230_SPI0_BASE          0x91584000ULL
#define K230_SPI1_BASE          0x91582000ULL
#define K230_SPI2_BASE          0x91583000ULL
#define K230_HI_SYS_BASE        0x91585000ULL
#define K230_SSI_CTRL_ADDR      (K230_HI_SYS_BASE + 0x68)
#define K230_FLASH_BASE         0xc0000000ULL
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
#define K230_SSI_CTRLR0_SRL             BIT(13)
#define K230_SSI_CTRLR0_SPI_FRF_SHIFT   22
#define K230_SSI_CTRLR0_SPI_FRF_MASK    (3U << K230_SSI_CTRLR0_SPI_FRF_SHIFT)

#define K230_SSI_TMOD_TR                0
#define K230_SSI_TMOD_TO                1
#define K230_SSI_TMOD_RO                2
#define K230_SSI_TMOD_EEPROM_READ       3

#define K230_SSI_FRF_DUAL               1
#define K230_SSI_FRF_QUAD               2
#define K230_SSI_FRF_OCTAL              3

#define K230_SSI_SPI_CTRLR0_TRANS_TYPE(v) ((v) & 0x3U)
#define K230_SSI_SPI_CTRLR0_ADDR_L(bits)  (((bits) / 4U) << 2)
#define K230_SSI_SPI_CTRLR0_XIP_MD_EN     BIT(7)
#define K230_SSI_SPI_CTRLR0_INST_L_8      (2U << 8)
#define K230_SSI_SPI_CTRLR0_WAIT(v)       (((v) & 0x1fU) << 11)
#define K230_SSI_SPI_CTRLR0_XIP_INST_EN   BIT(20)
#define K230_SSI_SPI_CTRLR0_SPI_DDR_EN    BIT(16)
#define K230_SSI_SPI_CTRLR0_INST_DDR_EN   BIT(17)
#define K230_SSI_SPI_CTRLR0_RXDS_EN       BIT(18)
#define K230_SSI_SPI_CTRLR0_RXDS_SIG_EN   BIT(25)
#define K230_SSI_SPI_CTRLR0_XIP_MBL_8     (2U << 26)

#define K230_SSI_SR_BUSY                BIT(0)
#define K230_SSI_SR_TFNF                BIT(1)
#define K230_SSI_SR_TFE                 BIT(2)
#define K230_SSI_SR_RFNE                BIT(3)
#define K230_SSI_SR_RFF                 BIT(4)
#define K230_SSI_SR_CMPLTD_DF_SHIFT     15
#define K230_SSI_SR_CMPLTD_DF_MASK      (0x1ffffU << 15)

#define K230_SSI_INT_TXE                BIT(0)
#define K230_SSI_INT_TXO                BIT(1)
#define K230_SSI_INT_RXU                BIT(2)
#define K230_SSI_INT_RXO                BIT(3)
#define K230_SSI_INT_RXF                BIT(4)
#define K230_SSI_INT_MST                BIT(5)
#define K230_SSI_INT_TXU                BIT(7)
#define K230_SSI_INT_AXIE               BIT(8)
#define K230_SSI_INT_DONE               BIT(11)

#define K230_SSI_IDMAE                  BIT(2)
#define K230_SSI_AINC                   BIT(6)
#define K230_SSI_DMA_ADDR               0x80201000ULL

#define K230_SSI_IRQ_TXE                0
#define K230_SSI_IRQ_TXO                1
#define K230_SSI_IRQ_RXF                2
#define K230_SSI_IRQ_RXO                3
#define K230_SSI_IRQ_TXU                4
#define K230_SSI_IRQ_RXU                5
#define K230_SSI_IRQ_MST                6
#define K230_SSI_IRQ_DONE               7
#define K230_SSI_IRQ_AXIE               8

#define K230_SSI_FIFO_DEPTH             256
#define K230_SSI_PIO_TX_BATCH           64
#define K230_SSI_FLASH_IMAGE_SIZE       (32 * MiB)
#define K230_SSI_FLASH_PATTERN_ADDR     0x100
#define K230_SSI_FLASH_HIGH_ADDR        0x1000100
#define K230_SSI_FLASH_PROGRAM_ADDR     0x22000
#define K230_SSI_FLASH_ERASE_ADDR       0x24000

typedef struct K230SsiInstance {
    uint64_t base;
    uint32_t num_cs;
    uint32_t spi_ctrlr0_reset;
    uint32_t first_irq;
} K230SsiInstance;

typedef struct K230SsiFlashImage {
    char *path;
} K230SsiFlashImage;

static const K230SsiInstance k230_ssi_instances[3] = {
    {
        .base = K230_SPI0_BASE,
        .num_cs = 1,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_FMC_RESET,
        .first_irq = 146,
    }, {
        .base = K230_SPI1_BASE,
        .num_cs = 5,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_SPI_RESET,
        .first_irq = 155,
    }, {
        .base = K230_SPI2_BASE,
        .num_cs = 5,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_SPI_RESET,
        .first_irq = 164,
    },
};

static QTestState *k230_ssi_start(void)
{
    return qtest_init("-machine k230");
}

static uint32_t k230_ssi_readl(QTestState *qts, uint64_t base, uint32_t offset)
{
    return qtest_readl(qts, base + offset);
}

static void k230_ssi_writel(QTestState *qts, uint64_t base,
                            uint32_t offset, uint32_t value)
{
    qtest_writel(qts, base + offset, value);
}

static void k230_ssi_disable(QTestState *qts, uint64_t base)
{
    k230_ssi_writel(qts, base, K230_SSI_SSIENR, 0);
}

static void k230_ssi_configure(QTestState *qts, uint64_t base,
                               uint32_t tmod, uint32_t dfs_bits, uint32_t ndf)
{
    uint32_t ctrlr0;

    g_assert_cmpuint(dfs_bits, >=, 4);
    g_assert_cmpuint(dfs_bits, <=, 32);
    g_assert_cmpuint(tmod, <=, K230_SSI_TMOD_EEPROM_READ);

    k230_ssi_disable(qts, base);
    ctrlr0 = (dfs_bits - 1) & K230_SSI_CTRLR0_DFS_MASK;
    ctrlr0 |= tmod << K230_SSI_CTRLR0_TMOD_SHIFT;
    k230_ssi_writel(qts, base, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, base, K230_SSI_CTRLR1, ndf);
    k230_ssi_writel(qts, base, K230_SSI_BAUDR, 2);
}

static void k230_ssi_enable_cs(QTestState *qts, uint64_t base, uint32_t ser)
{
    k230_ssi_writel(qts, base, K230_SSI_SER, ser);
    k230_ssi_writel(qts, base, K230_SSI_SSIENR, 1);
}

typedef struct RegisterMask {
    uint32_t offset;
    uint32_t mask;
} RegisterMask;

static void test_reset_values(void)
{
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];
        uint32_t sr = k230_ssi_readl(qts, inst->base, K230_SSI_SR);

        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_CTRLR0),
                        ==, K230_SSI_CTRLR0_RESET);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_SSIENR),
                        ==, 0);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_IMR),
                        ==, K230_SSI_IMR_RESET);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_AXIAWLEN),
                        ==, K230_SSI_AXILEN_RESET);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_AXIARLEN),
                        ==, K230_SSI_AXILEN_RESET);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_IDR),
                        ==, K230_SSI_IDR_RESET);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base,
                                      K230_SSI_VERSION_ID),
                        ==, K230_SSI_VERSION_RESET);
        g_assert_cmphex(sr & (K230_SSI_SR_BUSY | K230_SSI_SR_TFNF |
                             K230_SSI_SR_TFE | K230_SSI_SR_RFNE |
                             K230_SSI_SR_RFF),
                        ==, K230_SSI_SR_TFNF | K230_SSI_SR_TFE);
    }

    qtest_quit(qts);
}

static void test_profile_reset_values(void)
{
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];

        g_assert_cmphex(k230_ssi_readl(qts, inst->base,
                                      K230_SSI_SPI_CTRLR0),
                        ==, inst->spi_ctrlr0_reset);
    }

    qtest_quit(qts);
}

static void test_register_write_masks(void)
{
    static const RegisterMask masks[] = {
        { K230_SSI_CTRLR0, K230_SSI_CTRLR0_WRITABLE_MASK },
        { K230_SSI_CTRLR1, K230_SSI_CTRLR1_WRITABLE_MASK },
        { K230_SSI_MWCR, K230_SSI_MWCR_WRITABLE_MASK },
        { K230_SSI_BAUDR, K230_SSI_BAUDR_WRITABLE_MASK },
        { K230_SSI_TXFTLR, K230_SSI_TXFTLR_WRITABLE_MASK },
        { K230_SSI_RXFTLR, K230_SSI_RXFTLR_WRITABLE_MASK },
        { K230_SSI_IMR, K230_SSI_IMR_WRITABLE_MASK },
        { K230_SSI_DMACR, K230_SSI_DMACR_WRITABLE_MASK },
        { K230_SSI_AXIAWLEN, K230_SSI_AXILEN_WRITABLE_MASK },
        { K230_SSI_AXIARLEN, K230_SSI_AXILEN_WRITABLE_MASK },
        { K230_SSI_RX_SAMPLE_DELAY, K230_SSI_RX_SAMPLE_WRITABLE_MASK },
        { K230_SSI_SPI_CTRLR0, K230_SSI_SPI_CTRLR0_WRITABLE_MASK },
        { K230_SSI_DDR_DRIVE_EDGE, K230_SSI_DDR_EDGE_WRITABLE_MASK },
        { K230_SSI_XIP_MODE_BITS, K230_SSI_XIP_REG_WRITABLE_MASK },
        { K230_SSI_XIP_INCR_INST, K230_SSI_XIP_REG_WRITABLE_MASK },
        { K230_SSI_XIP_WRAP_INST, K230_SSI_XIP_REG_WRITABLE_MASK },
        { K230_SSI_SPIDR, K230_SSI_SPIDR_WRITABLE_MASK },
        { K230_SSI_SPIAR, UINT32_MAX },
        { K230_SSI_AXIAR0, UINT32_MAX },
        { K230_SSI_AXIAR1, UINT32_MAX },
    };
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(masks); i++) {
        k230_ssi_writel(qts, K230_SPI1_BASE, masks[i].offset, UINT32_MAX);
        g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE,
                                      masks[i].offset),
                        ==, masks[i].mask);
    }

    qtest_quit(qts);
}

static void test_ser_masks_follow_sdk_num_cs(void)
{
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];
        uint32_t expected = MAKE_64BIT_MASK(0, inst->num_cs);

        k230_ssi_writel(qts, inst->base, K230_SSI_SER, UINT32_MAX);
        g_assert_cmphex(k230_ssi_readl(qts, inst->base, K230_SSI_SER),
                        ==, expected);
    }

    qtest_quit(qts);
}

static void test_read_only_and_reserved_registers(void)
{
    static const uint32_t read_only[] = {
        K230_SSI_TXFLR, K230_SSI_RXFLR, K230_SSI_SR,
        K230_SSI_ISR, K230_SSI_RISR, K230_SSI_IDR,
        K230_SSI_VERSION_ID,
    };
    static const uint32_t razwi[] = {
        K230_SSI_XIP_CTRL, K230_SSI_XIP_SER, K230_SSI_XRXOICR,
        K230_SSI_XIP_TIMEOUT, K230_SSI_SPI_CTRLR1, K230_SSI_SPITECR,
        K230_SSI_RSVD_138, K230_SSI_RSVD_13C,
        K230_SSI_XIP_WRITE_INCR, K230_SSI_XIP_WRITE_WRAP,
        K230_SSI_XIP_WRITE_CTRL,
    };
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(read_only); i++) {
        uint32_t before = k230_ssi_readl(qts, K230_SPI1_BASE, read_only[i]);

        k230_ssi_writel(qts, K230_SPI1_BASE, read_only[i], UINT32_MAX);
        g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, read_only[i]),
                        ==, before);
    }

    for (int i = 0; i < ARRAY_SIZE(razwi); i++) {
        k230_ssi_writel(qts, K230_SPI1_BASE, razwi[i], UINT32_MAX);
        g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, razwi[i]),
                        ==, 0);
    }

    qtest_quit(qts);
}

static void test_enabled_write_contract(void)
{
    static const uint32_t locked[] = {
        K230_SSI_CTRLR0,
        K230_SSI_CTRLR1,
        K230_SSI_MWCR,
        K230_SSI_BAUDR,
        K230_SSI_SPI_CTRLR0,
    };
    static const RegisterMask enabled_writable[] = {
        { K230_SSI_TXFTLR, K230_SSI_TXFTLR_WRITABLE_MASK },
        { K230_SSI_RXFTLR, K230_SSI_RXFTLR_WRITABLE_MASK },
        { K230_SSI_DMACR, K230_SSI_DMACR_WRITABLE_MASK & ~BIT(2) },
        { K230_SSI_AXIAWLEN, K230_SSI_AXILEN_WRITABLE_MASK },
        { K230_SSI_AXIARLEN, K230_SSI_AXILEN_WRITABLE_MASK },
        { K230_SSI_RX_SAMPLE_DELAY, K230_SSI_RX_SAMPLE_WRITABLE_MASK },
        { K230_SSI_DDR_DRIVE_EDGE, K230_SSI_DDR_EDGE_WRITABLE_MASK },
        { K230_SSI_XIP_MODE_BITS, K230_SSI_XIP_REG_WRITABLE_MASK },
        { K230_SSI_XIP_INCR_INST, K230_SSI_XIP_REG_WRITABLE_MASK },
        { K230_SSI_XIP_WRAP_INST, K230_SSI_XIP_REG_WRITABLE_MASK },
        { K230_SSI_SPIDR, K230_SSI_SPIDR_WRITABLE_MASK },
        { K230_SSI_SPIAR, UINT32_MAX },
        { K230_SSI_AXIAR0, UINT32_MAX },
        { K230_SSI_AXIAR1, UINT32_MAX },
    };
    uint32_t locked_values[ARRAY_SIZE(locked)];
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);

    for (int i = 0; i < ARRAY_SIZE(locked); i++) {
        locked_values[i] = k230_ssi_readl(qts, K230_SPI1_BASE, locked[i]);
        k230_ssi_writel(qts, K230_SPI1_BASE, locked[i], UINT32_MAX);
        g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, locked[i]),
                        ==, locked_values[i]);
    }

    for (int i = 0; i < ARRAY_SIZE(enabled_writable); i++) {
        k230_ssi_writel(qts, K230_SPI1_BASE,
                        enabled_writable[i].offset,
                        enabled_writable[i].mask);
        g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE,
                                      enabled_writable[i].offset),
                        ==, enabled_writable[i].mask);
    }

    qtest_quit(qts);
}

static void test_internal_dma_registers_are_passive(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR,
                    K230_SSI_INT_DONE | K230_SSI_INT_AXIE);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPIDR, 0x9f);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPIAR, 0x12345678);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIAR0, 0x100000);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIAR1, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR,
                    K230_SSI_DMACR_WRITABLE_MASK);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    (K230_SSI_INT_DONE | K230_SSI_INT_AXIE), ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    (K230_SSI_INT_DONE | K230_SSI_INT_AXIE), ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_DONECR),
                    ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_AXIECR),
                    ==, 0);

    qtest_quit(qts);
}

static void test_system_reset_restores_contract(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, UINT32_MAX);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_INCR_INST, 0xeb);
    qtest_system_reset(qts);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0),
                    ==, K230_SSI_CTRLR0_RESET);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_IMR),
                    ==, K230_SSI_IMR_RESET);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE,
                                  K230_SSI_SPI_CTRLR0),
                    ==, K230_SSI_SPI_CTRLR0_FMC_RESET);

    qtest_quit(qts);
}

static void k230_ssi_write_frame(QTestState *qts, uint64_t base, uint32_t value)
{
    k230_ssi_writel(qts, base, K230_SSI_DR0, value);
}

static uint32_t k230_ssi_read_frame(QTestState *qts, uint64_t base)
{
    return k230_ssi_readl(qts, base, K230_SSI_DR0);
}

static void k230_ssi_wait_mask(QTestState *qts, uint64_t base, uint32_t offset,
                               uint32_t mask, uint32_t expected)
{
    for (int i = 0; i < 1000; i++) {
        uint32_t value = k230_ssi_readl(qts, base, offset);

        if ((value & mask) == expected) {
            return;
        }
        qtest_clock_step(qts, 1000);
    }

    g_assert_cmphex(k230_ssi_readl(qts, base, offset) & mask, ==, expected);
}

static uint32_t k230_ssi_frame_mask(uint32_t dfs_bits)
{
    g_assert_cmpuint(dfs_bits, >=, 4);
    g_assert_cmpuint(dfs_bits, <=, 32);
    return dfs_bits == 32 ? UINT32_MAX : MAKE_64BIT_MASK(0, dfs_bits);
}

static void configure_loopback(QTestState *qts, uint32_t tmod,
                               uint32_t dfs_bits, uint32_t ndf)
{
    uint32_t ctrlr0;

    k230_ssi_configure(qts, K230_SPI1_BASE, tmod, dfs_bits, ndf);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_CTRLR0,
                    ctrlr0 | K230_SSI_CTRLR0_SRL);
}

static void test_dr_aliases_share_one_fifo(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 32, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);

    for (int i = 0; i < K230_SSI_DR_COUNT; i++) {
        k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_DR(i), i + 1);
        g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE,
                                       K230_SSI_TXFLR), ==, i + 1);
    }

    k230_ssi_disable(qts, K230_SPI1_BASE);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, 0);
    qtest_quit(qts);
}

static void test_dr_write_while_disabled_is_ignored(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0xa5);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, 0);

    qtest_quit(qts);
}

static void test_fifo_depth_is_256_frames(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 32, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);

    for (int i = 0; i < K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, K230_SSI_FIFO_DEPTH);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR) &
                    K230_SSI_SR_TFNF, ==, 0);

    k230_ssi_write_frame(qts, K230_SPI1_BASE, UINT32_MAX);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, K230_SSI_FIFO_DEPTH);

    qtest_quit(qts);
}

static void test_dfs_masks_4_8_16_32_bit_frames(void)
{
    static const uint32_t widths[] = { 4, 8, 16, 32 };
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(widths); i++) {
        uint32_t width = widths[i];
        uint32_t expected = UINT32_MAX & k230_ssi_frame_mask(width);

        configure_loopback(qts, K230_SSI_TMOD_TR, width, 0);
        k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
        k230_ssi_write_frame(qts, K230_SPI1_BASE, UINT32_MAX);
        k230_ssi_wait_mask(qts, K230_SPI1_BASE, K230_SSI_SR,
                           K230_SSI_SR_RFNE, K230_SSI_SR_RFNE);
        g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI1_BASE),
                        ==, expected);
        k230_ssi_disable(qts, K230_SPI1_BASE);
    }

    qtest_quit(qts);
}

static void test_tmod_transmit_and_receive(void)
{
    QTestState *qts = k230_ssi_start();

    configure_loopback(qts, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0xa5);
    k230_ssi_wait_mask(qts, K230_SPI1_BASE, K230_SSI_SR,
                       K230_SSI_SR_RFNE, K230_SSI_SR_RFNE);
    g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI1_BASE), ==, 0xa5);

    qtest_quit(qts);
}

static void test_tmod_transmit_only_discards_rx(void)
{
    QTestState *qts = k230_ssi_start();

    configure_loopback(qts, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0xa5);
    k230_ssi_wait_mask(qts, K230_SPI1_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RXFLR),
                     ==, 0);

    qtest_quit(qts);
}

static void test_tmod_transmit_only_refills_in_batches(void)
{
    QTestState *qts = k230_ssi_start();
    const unsigned int total_frames = K230_SSI_FIFO_DEPTH * 3;
    unsigned int written = K230_SSI_FIFO_DEPTH;
    uint32_t txflr;
    uint32_t status;

    configure_loopback(qts, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SSIENR, 1);
    for (int i = 0; i < K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }

    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SER, 1);
    txflr = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR);
    g_assert_cmpuint(txflr, ==,
                     K230_SSI_FIFO_DEPTH - K230_SSI_PIO_TX_BATCH);
    status = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR);
    g_assert_cmphex(status & K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    while (written < total_frames) {
        unsigned int refill = MIN(total_frames - written,
                                  K230_SSI_FIFO_DEPTH - txflr);

        for (int i = 0; i < refill; i++) {
            k230_ssi_write_frame(qts, K230_SPI1_BASE, written + i);
        }
        written += refill;

        txflr = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR);
        g_assert_cmpuint(txflr, >, 0);
    }

    while (k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR)) {
        /* TXFLR reads advance one batch after returning their snapshot. */
    }
    status = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmphex(status & K230_SSI_SR_BUSY, ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RXFLR),
                     ==, 0);

    qtest_quit(qts);
}

static void test_ser_deselect_aborts_transmit_only(void)
{
    QTestState *qts = k230_ssi_start();

    configure_loopback(qts, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SSIENR, 1);
    for (int i = 0; i < K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SER, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SER, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    qtest_quit(qts);
}

static void test_tmod_receive_only_uses_ndf(void)
{
    QTestState *qts = k230_ssi_start();

    configure_loopback(qts, K230_SSI_TMOD_RO, 8, 3);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0);
    k230_ssi_wait_mask(qts, K230_SPI1_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, 4);
    for (int i = 0; i < 4; i++) {
        g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI1_BASE), ==, 0);
    }

    qtest_quit(qts);
}

static void test_tmod_eeprom_read_has_separate_rx_count(void)
{
    QTestState *qts = k230_ssi_start();

    configure_loopback(qts, K230_SSI_TMOD_EEPROM_READ, 8, 2);

    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0x03);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0x00);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SER, 1);
    k230_ssi_wait_mask(qts, K230_SPI1_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, 3);

    qtest_quit(qts);
}

static void test_disable_stops_transfer_and_clears_fifos(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);
    for (int i = 0; i < 8; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     >, 0);

    k230_ssi_disable(qts, K230_SPI1_BASE);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR) &
                    (K230_SSI_SR_BUSY | K230_SSI_SR_TFE |
                     K230_SSI_SR_TFNF | K230_SSI_SR_RFNE),
                    ==, K230_SSI_SR_TFE | K230_SSI_SR_TFNF);

    qtest_quit(qts);
}

static void test_dynamic_status_during_paused_rx_only(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t status;

    configure_loopback(qts, K230_SSI_TMOD_RO, 8, K230_SSI_FIFO_DEPTH);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE,
                                    K230_SSI_TXFLR), ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE,
                                    K230_SSI_RXFLR), ==,
                     K230_SSI_FIFO_DEPTH);
    status = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR);
    g_assert_cmphex(status & (K230_SSI_SR_BUSY | K230_SSI_SR_TFNF |
                              K230_SSI_SR_TFE | K230_SSI_SR_RFNE |
                              K230_SSI_SR_RFF), ==,
                    K230_SSI_SR_BUSY | K230_SSI_SR_TFNF |
                    K230_SSI_SR_TFE | K230_SSI_SR_RFNE |
                    K230_SSI_SR_RFF);

    g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI1_BASE), ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE,
                                    K230_SSI_RXFLR), ==,
                     K230_SSI_FIFO_DEPTH);
    status = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR);
    g_assert_cmphex(status & (K230_SSI_SR_BUSY | K230_SSI_SR_TFNF |
                              K230_SSI_SR_TFE | K230_SSI_SR_RFNE |
                              K230_SSI_SR_RFF), ==,
                    K230_SSI_SR_TFNF | K230_SSI_SR_TFE |
                    K230_SSI_SR_RFNE | K230_SSI_SR_RFF);

    for (int i = 0; i < K230_SSI_FIFO_DEPTH; i++) {
        g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI1_BASE), ==, 0);
    }

    status = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_SR);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE,
                                    K230_SSI_RXFLR), ==, 0);
    g_assert_cmphex(status & (K230_SSI_SR_BUSY | K230_SSI_SR_TFNF |
                              K230_SSI_SR_TFE | K230_SSI_SR_RFNE |
                              K230_SSI_SR_RFF), ==,
                    K230_SSI_SR_TFNF | K230_SSI_SR_TFE);

    qtest_quit(qts);
}

static void test_watermark_and_mask_relationship(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t ctrlr0;

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_TXFTLR, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_IMR, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SSIENR, 1);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_TXE, ==, 0);

    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_IMR,
                    K230_SSI_INT_TXE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);

    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0x5a);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SER, BIT(0));
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);

    k230_ssi_disable(qts, K230_SPI1_BASE);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_CTRLR0,
                    ctrlr0 | K230_SSI_CTRLR0_SRL);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_RXFTLR, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_IMR,
                    K230_SSI_INT_RXF);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, BIT(0));
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0xa5);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXF, ==, K230_SSI_INT_RXF);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_RXF, ==, K230_SSI_INT_RXF);
    g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI1_BASE), ==, 0xa5);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXF, ==, 0);

    qtest_quit(qts);
}

static void test_rx_underflow_latches_and_read_clears(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_IMR,
                    K230_SSI_INT_RXU);
    (void)k230_ssi_read_frame(qts, K230_SPI1_BASE);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, K230_SSI_INT_RXU);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_RXU, ==, K230_SSI_INT_RXU);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RXUICR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, 0);

    qtest_quit(qts);
}

static void test_tx_overflow_latches_and_txeicr_clears(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 32, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_SSIENR, 1);
    for (int i = 0; i <= K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXO, ==, K230_SSI_INT_TXO);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXEICR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXO, ==, 0);

    qtest_quit(qts);
}

static void test_rx_overflow_latches_and_rxoicr_clears(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t ctrlr0;

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 8, 0);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_CTRLR0,
                    ctrlr0 | K230_SSI_CTRLR0_SRL);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
    for (int i = 0; i <= K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RXFLR),
                     ==, K230_SSI_FIFO_DEPTH);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXO, ==, K230_SSI_INT_RXO);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RXOICR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXO, ==, 0);

    qtest_quit(qts);
}

static void test_icr_clear_scope(void)
{
    QTestState *qts = k230_ssi_start();

    (void)k230_ssi_read_frame(qts, K230_SPI1_BASE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ICR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_DONECR),
                    ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_AXIECR),
                    ==, 0);

    qtest_quit(qts);
}

static void test_reset_clears_latched_causes(void)
{
    QTestState *qts = k230_ssi_start();

    (void)k230_ssi_read_frame(qts, K230_SPI1_BASE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, K230_SSI_INT_RXU);

    qtest_system_reset(qts);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);

    qtest_quit(qts);
}

static void test_mst_txu_done_axie_inactive_without_causes(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t inactive = K230_SSI_INT_MST | K230_SSI_INT_TXU |
                        K230_SSI_INT_DONE | K230_SSI_INT_AXIE;

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    inactive, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_MSTICR),
                    ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXEICR),
                    ==, 0);

    qtest_quit(qts);
}

static bool k230_ssi_plic_pending(QTestState *qts, uint32_t irq)
{
    uint64_t addr = K230_PLIC_BASE + K230_PLIC_PENDING_BASE +
                    (irq / 32) * sizeof(uint32_t);

    return qtest_readl(qts, addr) & BIT(irq % 32);
}

static void test_plic_txe_reset_routing(void)
{
    QTestState *qts = k230_ssi_start();

    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];

        g_assert_true(k230_ssi_plic_pending(qts,
                                           inst->first_irq +
                                           K230_SSI_IRQ_TXE));
        g_assert_false(k230_ssi_plic_pending(qts,
                                            inst->first_irq +
                                            K230_SSI_IRQ_DONE));
        g_assert_false(k230_ssi_plic_pending(qts,
                                            inst->first_irq +
                                            K230_SSI_IRQ_AXIE));
    }

    qtest_quit(qts);
}

static void test_plic_rxu_routing_and_instance_isolation(void)
{
    for (int target = 0; target < ARRAY_SIZE(k230_ssi_instances); target++) {
        QTestState *qts = k230_ssi_start();
        const K230SsiInstance *inst = &k230_ssi_instances[target];

        for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
            k230_ssi_writel(qts, k230_ssi_instances[i].base,
                            K230_SSI_IMR, 0);
        }
        k230_ssi_writel(qts, inst->base, K230_SSI_IMR, K230_SSI_INT_RXU);
        (void)k230_ssi_read_frame(qts, inst->base);

        g_assert_true(k230_ssi_plic_pending(qts,
                                           inst->first_irq +
                                           K230_SSI_IRQ_RXU));
        for (int other = 0; other < ARRAY_SIZE(k230_ssi_instances); other++) {
            if (other == target) {
                continue;
            }
            g_assert_false(k230_ssi_plic_pending(
                qts, k230_ssi_instances[other].first_irq +
                     K230_SSI_IRQ_RXU));
        }

        qtest_quit(qts);
    }
}

static void test_plic_rxf_routing(void)
{
    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];
        QTestState *qts = k230_ssi_start();
        uint32_t ctrlr0;

        k230_ssi_configure(qts, inst->base, K230_SSI_TMOD_TR, 8, 0);
        ctrlr0 = k230_ssi_readl(qts, inst->base, K230_SSI_CTRLR0);
        k230_ssi_writel(qts, inst->base, K230_SSI_CTRLR0,
                        ctrlr0 | K230_SSI_CTRLR0_SRL);
        k230_ssi_writel(qts, inst->base, K230_SSI_RXFTLR, 0);
        k230_ssi_writel(qts, inst->base, K230_SSI_IMR, K230_SSI_INT_RXF);
        k230_ssi_enable_cs(qts, inst->base, BIT(0));
        k230_ssi_write_frame(qts, inst->base, 0x5a);

        g_assert_true(k230_ssi_plic_pending(qts,
                                           inst->first_irq +
                                           K230_SSI_IRQ_RXF));
        qtest_quit(qts);
    }
}

static void test_plic_txo_routing(void)
{
    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];
        QTestState *qts = k230_ssi_start();

        k230_ssi_configure(qts, inst->base, K230_SSI_TMOD_TR, 32, 0);
        k230_ssi_writel(qts, inst->base, K230_SSI_IMR,
                        K230_SSI_INT_TXO);
        k230_ssi_writel(qts, inst->base, K230_SSI_SSIENR, 1);
        for (int frame = 0; frame <= K230_SSI_FIFO_DEPTH; frame++) {
            k230_ssi_write_frame(qts, inst->base, frame);
        }

        g_assert_true(k230_ssi_plic_pending(qts,
                                           inst->first_irq +
                                           K230_SSI_IRQ_TXO));
        qtest_quit(qts);
    }
}

static void test_plic_rxo_routing(void)
{
    for (int i = 0; i < ARRAY_SIZE(k230_ssi_instances); i++) {
        const K230SsiInstance *inst = &k230_ssi_instances[i];
        QTestState *qts = k230_ssi_start();
        uint32_t ctrlr0;

        k230_ssi_configure(qts, inst->base, K230_SSI_TMOD_TR, 8, 0);
        ctrlr0 = k230_ssi_readl(qts, inst->base, K230_SSI_CTRLR0);
        k230_ssi_writel(qts, inst->base, K230_SSI_CTRLR0,
                        ctrlr0 | K230_SSI_CTRLR0_SRL);
        k230_ssi_writel(qts, inst->base, K230_SSI_IMR, K230_SSI_INT_RXO);
        k230_ssi_enable_cs(qts, inst->base, BIT(0));
        for (int frame = 0; frame <= K230_SSI_FIFO_DEPTH; frame++) {
            k230_ssi_write_frame(qts, inst->base, frame);
        }

        g_assert_true(k230_ssi_plic_pending(qts,
                                           inst->first_irq +
                                           K230_SSI_IRQ_RXO));
        qtest_quit(qts);
    }
}

#define FLASH_CMD_JEDEC         0x9f

static void assert_enhanced_config_rejected(QTestState *qts,
                                            uint32_t tmod, uint32_t frf,
                                            uint32_t trans_type,
                                            uint32_t extra_spi_ctrlr0)
{
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    k230_ssi_configure(qts, K230_SPI0_BASE, tmod, 8, 0);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= frf << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(trans_type) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 extra_spi_ctrlr0;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_enable_cs(qts, K230_SPI0_BASE, BIT(0));
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_JEDEC);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 1);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void test_unsupported_enhanced_configs_do_not_transfer(void)
{
    QTestState *qts = k230_ssi_start();

    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_RO,
                                    K230_SSI_FRF_OCTAL, 0, 0);
    assert_enhanced_config_rejected(
        qts, K230_SSI_TMOD_RO, K230_SSI_FRF_QUAD, 0,
        K230_SSI_SPI_CTRLR0_SPI_DDR_EN |
        K230_SSI_SPI_CTRLR0_INST_DDR_EN |
        K230_SSI_SPI_CTRLR0_RXDS_EN |
        K230_SSI_SPI_CTRLR0_RXDS_SIG_EN);
    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_RO,
                                    K230_SSI_FRF_QUAD, 3, 0);
    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_TR,
                                    K230_SSI_FRF_QUAD, 0, 0);
    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_EEPROM_READ,
                                    K230_SSI_FRF_QUAD, 0, 0);

    qtest_quit(qts);
}

static void test_supported_enhanced_command_is_parsed(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_RO, 8, 3);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 |= K230_SSI_FRF_QUAD << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(1) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_WAIT(8);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_enable_cs(qts, K230_SPI0_BASE, BIT(0));

    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_JEDEC);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, 0x123456);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 4);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);
    for (int i = 0; i < 4; i++) {
        g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI0_BASE), ==, 0);
    }

    qtest_quit(qts);
}

static void write_exact(int fd, const void *buf, size_t len, off_t offset)
{
    ssize_t ret = pwrite(fd, buf, len, offset);

    g_assert_cmpint(ret, ==, len);
}

static void k230_ssi_flash_image_init(K230SsiFlashImage *image)
{
    static const uint8_t low_pattern[] = {
        0xa5, 0x5a, 0x3c, 0xc3, 0x11, 0x22, 0x33, 0x44,
    };
    static const uint8_t high_pattern[] = { 0x71, 0x72, 0x73, 0x74 };
    uint8_t erased[4096];
    uint8_t dirty[4096];
    int fd;

    memset(erased, 0xff, sizeof(erased));
    memset(dirty, 0x00, sizeof(dirty));

    image->path = NULL;
    fd = g_file_open_tmp("qtest.k230.w25q256.XXXXXX", &image->path, NULL);
    g_assert_cmpint(fd, >=, 0);
    g_assert_cmpint(ftruncate(fd, K230_SSI_FLASH_IMAGE_SIZE), ==, 0);

    write_exact(fd, low_pattern, sizeof(low_pattern),
                K230_SSI_FLASH_PATTERN_ADDR);
    write_exact(fd, high_pattern, sizeof(high_pattern),
                K230_SSI_FLASH_HIGH_ADDR);
    write_exact(fd, erased, sizeof(erased), K230_SSI_FLASH_PROGRAM_ADDR);
    write_exact(fd, dirty, sizeof(dirty), K230_SSI_FLASH_ERASE_ADDR);
    close(fd);
}

static void k230_ssi_flash_image_clear(K230SsiFlashImage *image)
{
    if (image->path) {
        unlink(image->path);
        g_clear_pointer(&image->path, g_free);
    }
}

static QTestState *k230_ssi_start_with_flash(K230SsiFlashImage *image)
{
    k230_ssi_flash_image_init(image);
    return qtest_initf("-machine k230,spi-flash=w25q256 "
                       "-drive file=%s,format=raw,if=mtd",
                       image->path);
}

#define FLASH_CMD_WREN          0x06
#define FLASH_CMD_RDSR          0x05
#define FLASH_CMD_READ          0x03
#define FLASH_CMD_READ4         0x13
#define FLASH_CMD_FAST_READ     0x0b
#define FLASH_CMD_QUAD_OUT      0x6b
#define FLASH_CMD_QUAD_IO       0xeb
#define FLASH_CMD_PP            0x02
#define FLASH_CMD_SE            0x20
#define FLASH_SR_WIP            BIT(0)

#define K230_SSI_SPI_CTRLR0_INST_L_16    (3U << 8)

static void flash_write_transaction(QTestState *qts,
                                    const uint8_t *command,
                                    size_t command_len)
{
    g_assert_nonnull(command);
    g_assert_cmpuint(command_len, >, 0);
    g_assert_cmpuint(command_len, <=, K230_SSI_FIFO_DEPTH);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);

    for (size_t i = 0; i < command_len; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, command[i]);
    }

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_TXFLR,
                       UINT32_MAX, 0);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);

    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void flash_write_streaming(QTestState *qts,
                                  const uint8_t *command,
                                  size_t command_len)
{
    size_t written = MIN(command_len, K230_SSI_FIFO_DEPTH);
    uint32_t status;

    g_assert_nonnull(command);
    g_assert_cmpuint(command_len, >, K230_SSI_FIFO_DEPTH);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    for (size_t i = 0; i < written; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, command[i]);
    }

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    status = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR);
    g_assert_cmphex(status & K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    while (written < command_len) {
        uint32_t txflr = k230_ssi_readl(qts, K230_SPI0_BASE,
                                        K230_SSI_TXFLR);
        size_t count = MIN(command_len - written,
                           K230_SSI_FIFO_DEPTH - txflr);

        g_assert_cmpuint(count, >, 0);
        for (size_t i = 0; i < count; i++) {
            k230_ssi_write_frame(qts, K230_SPI0_BASE,
                                 command[written + i]);
        }
        written += count;
    }

    while (k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR)) {
        /* TXFLR snapshots also advance the next PIO batch. */
    }
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void flash_read_transaction(QTestState *qts,
                                   const uint8_t *command,
                                   size_t command_len,
                                   uint8_t *data, size_t data_len)
{
    g_assert_nonnull(command);
    g_assert_nonnull(data);
    g_assert_cmpuint(command_len, >, 0);
    g_assert_cmpuint(command_len, <=, K230_SSI_FIFO_DEPTH);
    g_assert_cmpuint(data_len, >, 0);
    g_assert_cmpuint(data_len, <, K230_SSI_FIFO_DEPTH);

    k230_ssi_configure(qts, K230_SPI0_BASE,
                       K230_SSI_TMOD_EEPROM_READ, 8, data_len - 1);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);

    for (size_t i = 0; i < command_len; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, command[i]);
    }

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, data_len);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);

    for (size_t i = 0; i < data_len; i++) {
        data[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }

    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void flash_read(QTestState *qts, uint8_t opcode, uint32_t address,
                       unsigned int addr_bytes, unsigned int dummy_bytes,
                       uint8_t *data, size_t len)
{
    size_t prefix = 1 + addr_bytes + dummy_bytes;
    g_autofree uint8_t *command = g_new0(uint8_t, prefix);

    command[0] = opcode;
    for (unsigned int i = 0; i < addr_bytes; i++) {
        command[1 + i] = address >> (8 * (addr_bytes - i - 1));
    }
    memset(command + 1 + addr_bytes, 0xff, dummy_bytes);
    flash_read_transaction(qts, command, prefix, data, len);
}

static uint8_t flash_read_status(QTestState *qts)
{
    uint8_t command = FLASH_CMD_RDSR;
    uint8_t status;

    flash_read_transaction(qts, &command, 1, &status, 1);
    return status;
}

static void flash_wait_ready(QTestState *qts)
{
    for (int i = 0; i < 1000; i++) {
        if (!(flash_read_status(qts) & FLASH_SR_WIP)) {
            return;
        }
        qtest_clock_step(qts, 1000000);
    }

    g_assert_cmphex(flash_read_status(qts) & FLASH_SR_WIP, ==, 0);
}

static void flash_write_enable(QTestState *qts)
{
    uint8_t cmd = FLASH_CMD_WREN;

    flash_write_transaction(qts, &cmd, 1);
}

static void test_jedec_id(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t command = FLASH_CMD_JEDEC;
    uint8_t id[6];

    flash_read_transaction(qts, &command, 1, id, sizeof(id));
    g_assert_cmphex(id[0], ==, 0xef);
    g_assert_cmphex(id[1], ==, 0x40);
    g_assert_cmphex(id[2], ==, 0x19);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_flash_read_3byte_address(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t data[8];
    static const uint8_t expected[8] = {
        0xa5, 0x5a, 0x3c, 0xc3, 0x11, 0x22, 0x33, 0x44,
    };

    flash_read(qts, FLASH_CMD_READ, K230_SSI_FLASH_PATTERN_ADDR,
               3, 0, data, sizeof(data));
    g_assert_cmpmem(data, sizeof(data), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_flash_read_4byte_address(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t data[4];
    static const uint8_t expected[4] = { 0x71, 0x72, 0x73, 0x74 };

    flash_read(qts, FLASH_CMD_READ4, K230_SSI_FLASH_HIGH_ADDR,
               4, 0, data, sizeof(data));
    g_assert_cmpmem(data, sizeof(data), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_page_program_and_readback(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t tx[4 + sizeof(payload)];
    uint8_t actual[sizeof(payload)];
    uint32_t addr = K230_SSI_FLASH_PROGRAM_ADDR;

    flash_write_enable(qts);
    tx[0] = FLASH_CMD_PP;
    tx[1] = addr >> 16;
    tx[2] = addr >> 8;
    tx[3] = addr;
    memcpy(tx + 4, payload, sizeof(payload));
    flash_write_transaction(qts, tx, sizeof(tx));
    flash_wait_ready(qts);

    flash_read(qts, FLASH_CMD_READ, addr, 3, 0, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), payload, sizeof(payload));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_native_cs_full_page_program(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t tx[4 + 256];
    uint8_t actual[256];
    uint32_t addr = K230_SSI_FLASH_PROGRAM_ADDR;

    tx[0] = FLASH_CMD_PP;
    tx[1] = addr >> 16;
    tx[2] = addr >> 8;
    tx[3] = addr;
    for (size_t i = 0; i < sizeof(actual); i++) {
        tx[4 + i] = i ^ 0x5a;
    }

    flash_write_enable(qts);
    flash_write_streaming(qts, tx, sizeof(tx));
    flash_wait_ready(qts);

    for (size_t offset = 0; offset < sizeof(actual); offset += 64) {
        flash_read(qts, FLASH_CMD_READ, addr + offset, 3, 0,
                   actual + offset, 64);
    }
    g_assert_cmpmem(actual, sizeof(actual), tx + 4, sizeof(actual));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_sector_erase_and_readback(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t addr = K230_SSI_FLASH_ERASE_ADDR;
    uint8_t tx[] = {
        FLASH_CMD_SE, addr >> 16, addr >> 8, addr,
    };
    uint8_t actual[16];
    uint8_t expected[16];

    memset(expected, 0xff, sizeof(expected));
    flash_write_enable(qts);
    flash_write_transaction(qts, tx, sizeof(tx));
    flash_wait_ready(qts);

    flash_read(qts, FLASH_CMD_READ, addr, 3, 0, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_chip_select_restarts_command(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t command = FLASH_CMD_JEDEC;
    uint8_t first;
    uint8_t second;

    flash_read_transaction(qts, &command, 1, &first, 1);
    flash_read_transaction(qts, &command, 1, &second, 1);
    g_assert_cmphex(first, ==, 0xef);
    g_assert_cmphex(second, ==, 0xef);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_failed_enhanced_reset_falls_back_to_read_id(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;
    uint32_t fifo;
    uint8_t id[6];

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    for (fifo = 1; fifo < K230_SSI_FIFO_DEPTH; fifo++) {
        k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_TXFTLR, fifo);
        if (k230_ssi_readl(qts, K230_SPI0_BASE,
                           K230_SSI_TXFTLR) != fifo) {
            break;
        }
    }
    g_assert_cmpuint(fifo, ==, K230_SSI_FIFO_DEPTH);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_TXFTLR, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_RXFTLR, fifo - 1);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= K230_SSI_FRF_OCTAL << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(2) |
                 K230_SSI_SPI_CTRLR0_INST_L_16 |
                 K230_SSI_SPI_CTRLR0_SPI_DDR_EN |
                 K230_SSI_SPI_CTRLR0_INST_DDR_EN;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, 0x6666);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 1);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);

    k230_ssi_configure(qts, K230_SPI0_BASE,
                       K230_SSI_TMOD_EEPROM_READ, 8, ARRAY_SIZE(id) - 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_JEDEC);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, ARRAY_SIZE(id));
    for (size_t i = 0; i < ARRAY_SIZE(id); i++) {
        id[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    g_assert_cmphex(id[0], ==, 0xef);
    g_assert_cmphex(id[1], ==, 0x40);
    g_assert_cmphex(id[2], ==, 0x19);

    k230_ssi_disable(qts, K230_SPI0_BASE);
    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

#define FLASH_CMD_DUAL_OUT      0x3b
#define FLASH_CMD_QUAD_PP       0x32

static void configure_enhanced_transfer(QTestState *qts, uint32_t tmod,
                                        uint32_t frf, uint32_t trans_type,
                                        uint32_t wait_cycles,
                                        bool mode_bits_enabled,
                                        size_t data_frames)
{
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    g_assert_cmpuint(data_frames, >, 0);
    g_assert_cmpuint(data_frames, <=, UINT16_MAX + 1ULL);

    k230_ssi_configure(qts, K230_SPI0_BASE, tmod, 8, data_frames - 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);

    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= frf << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);

    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(trans_type) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_WAIT(wait_cycles);
    if (mode_bits_enabled) {
        spi_ctrlr0 |= K230_SSI_SPI_CTRLR0_XIP_MD_EN |
                      K230_SSI_SPI_CTRLR0_XIP_MBL_8;
    }
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
}

static void start_enhanced_transfer(QTestState *qts,
                                    uint8_t opcode, uint32_t address)
{
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, opcode);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, address);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
}

static void configure_enhanced_read(QTestState *qts, uint32_t frf,
                                    uint32_t trans_type, uint32_t wait_cycles,
                                    bool mode_bits_enabled,
                                    uint8_t opcode, uint32_t address,
                                    size_t read_len)
{
    configure_enhanced_transfer(qts, K230_SSI_TMOD_RO, frf, trans_type,
                                wait_cycles, mode_bits_enabled, read_len);
    start_enhanced_transfer(qts, opcode, address);
}

static void standard_write_transaction(QTestState *qts,
                                       const uint8_t *frames, size_t len)
{
    g_assert_nonnull(frames);
    g_assert_cmpuint(len, >, 0);
    g_assert_cmpuint(len, <=, K230_SSI_FIFO_DEPTH);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);

    for (size_t i = 0; i < len; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, frames[i]);
    }
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_TXFLR,
                       UINT32_MAX, 0);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static uint8_t standard_read_status(QTestState *qts)
{
    uint8_t status;

    k230_ssi_configure(qts, K230_SPI0_BASE,
                       K230_SSI_TMOD_EEPROM_READ, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_RDSR);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, 1);
    status = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    k230_ssi_disable(qts, K230_SPI0_BASE);
    return status;
}

static void standard_wait_ready(QTestState *qts)
{
    for (int i = 0; i < 1000; i++) {
        if (!(standard_read_status(qts) & FLASH_SR_WIP)) {
            return;
        }
        qtest_clock_step(qts, 1000000);
    }

    g_assert_cmphex(standard_read_status(qts) & FLASH_SR_WIP, ==, 0);
}

static void read_enhanced_result(QTestState *qts, uint8_t *data, size_t len)
{
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, len);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);

    for (size_t i = 0; i < len; i++) {
        data[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void assert_enhanced_read(QTestState *qts, uint32_t frf,
                                 uint8_t opcode, uint32_t trans_type)
{
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    configure_enhanced_read(qts, frf, trans_type, 8, false,
                            opcode, K230_SSI_FLASH_PATTERN_ADDR,
                            ARRAY_SIZE(actual));
    read_enhanced_result(qts, actual, ARRAY_SIZE(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));
}

static void test_dual_and_quad_output_read(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    assert_enhanced_read(qts, K230_SSI_FRF_DUAL,
                         FLASH_CMD_DUAL_OUT, 0);
    assert_enhanced_read(qts, K230_SSI_FRF_QUAD,
                         FLASH_CMD_QUAD_OUT, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_page_program_streaming(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t actual[ARRAY_SIZE(payload)];
    uint8_t wren = FLASH_CMD_WREN;
    uint32_t addr = K230_SSI_FLASH_PROGRAM_ADDR;

    standard_write_transaction(qts, &wren, 1);
    configure_enhanced_transfer(qts, K230_SSI_TMOD_TO,
                                K230_SSI_FRF_QUAD, 0, 0, false,
                                ARRAY_SIZE(payload));
    start_enhanced_transfer(qts, FLASH_CMD_QUAD_PP, addr);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_TXFLR), ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    for (size_t i = 0; i < ARRAY_SIZE(payload) / 2; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, payload[i]);
    }
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    for (size_t i = ARRAY_SIZE(payload) / 2;
         i < ARRAY_SIZE(payload); i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, payload[i]);
    }
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_TXFLR,
                       UINT32_MAX, 0);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_RXFLR), ==, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);

    standard_wait_ready(qts);
    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 0, 8, false,
                            FLASH_CMD_QUAD_OUT, addr,
                            ARRAY_SIZE(actual));
    read_enhanced_result(qts, actual, ARRAY_SIZE(actual));
    g_assert_cmpmem(actual, sizeof(actual), payload, sizeof(payload));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_enhanced_rx_fifo_resume(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t status;

    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 0, 8, false,
                            FLASH_CMD_QUAD_OUT,
                            K230_SSI_FLASH_PATTERN_ADDR,
                            K230_SSI_FIFO_DEPTH + 1);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_RXFLR), ==,
                     K230_SSI_FIFO_DEPTH);
    status = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR);
    g_assert_cmphex(status & (K230_SSI_SR_BUSY | K230_SSI_SR_RFF), ==,
                    K230_SSI_SR_BUSY | K230_SSI_SR_RFF);

    g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI0_BASE), ==, 0xa5);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_RXFLR), ==,
                     K230_SSI_FIFO_DEPTH);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    for (int i = 0; i < K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    k230_ssi_disable(qts, K230_SPI0_BASE);
    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_enhanced_prefix_is_atomic(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t data[4];

    configure_enhanced_transfer(qts, K230_SSI_TMOD_RO,
                                K230_SSI_FRF_QUAD, 0, 8, false,
                                ARRAY_SIZE(data));
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));

    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_QUAD_OUT);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_TXFLR), ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    k230_ssi_write_frame(qts, K230_SPI0_BASE,
                         K230_SSI_FLASH_PATTERN_ADDR);
    read_enhanced_result(qts, data, ARRAY_SIZE(data));
    g_assert_cmphex(data[0], ==, 0xa5);
    g_assert_cmphex(data[1], ==, 0x5a);
    g_assert_cmphex(data[2], ==, 0x3c);
    g_assert_cmphex(data[3], ==, 0xc3);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_io_mode_bits_and_dummy(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_MODE_BITS, 0xa5);
    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 1, 4, true,
                            FLASH_CMD_QUAD_IO,
                            K230_SSI_FLASH_PATTERN_ADDR,
                            ARRAY_SIZE(actual));
    read_enhanced_result(qts, actual, ARRAY_SIZE(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void configure_idma(QTestState *qts, uint32_t tmod,
                           uint32_t trans_type, uint32_t wait_cycles,
                           uint8_t opcode, uint32_t flash_address,
                           uint64_t dma_address, size_t length,
                           uint32_t dmacr, bool dmacr_last)
{
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    g_assert_cmpuint(length, >, 0);
    g_assert_cmpuint(length, <=, UINT16_MAX + 1ULL);

    k230_ssi_configure(qts, K230_SPI0_BASE, tmod, 8, length - 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= K230_SSI_FRF_QUAD << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);

    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(trans_type) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_WAIT(wait_cycles);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    if (!dmacr_last) {
        k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR, dmacr);
    }
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPIDR, opcode);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPIAR, flash_address);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIAR0, dma_address);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIAR1,
                    dma_address >> 32);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    if (dmacr_last) {
        k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR, dmacr);
    }
}

static void assert_idma_stopped(QTestState *qts, size_t completed)
{
    uint32_t status = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SSIENR),
                    ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SER),
                    ==, BIT(0));
    g_assert_cmpuint((status & K230_SSI_SR_CMPLTD_DF_MASK) >>
                     K230_SSI_SR_CMPLTD_DF_SHIFT, ==, completed);
}

static void test_idma_quad_output_read_and_done(void)
{
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t actual[ARRAY_SIZE(expected)];

    qtest_memset(qts, K230_SSI_DMA_ADDR, 0, sizeof(actual));
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR, 0);
    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(actual), K230_SSI_IDMAE | K230_SSI_AINC, false);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, K230_SSI_INT_DONE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_DONE, ==, 0);
    g_assert_false(k230_ssi_plic_pending(
        qts, k230_ssi_instances[0].first_irq + K230_SSI_IRQ_DONE));

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR,
                    K230_SSI_INT_DONE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_DONE, ==, K230_SSI_INT_DONE);
    g_assert_true(k230_ssi_plic_pending(
        qts, k230_ssi_instances[0].first_irq + K230_SSI_IRQ_DONE));
    assert_idma_stopped(qts, sizeof(actual));

    qtest_memread(qts, K230_SSI_DMA_ADDR, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DONECR, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, K230_SSI_INT_DONE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_DONECR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, 0);

    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(actual), K230_SSI_IDMAE | K230_SSI_AINC, false);
    assert_idma_stopped(qts, sizeof(actual));
    qtest_system_reset(qts);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    (K230_SSI_INT_DONE | K230_SSI_INT_AXIE), ==, 0);
    g_assert_cmpuint((k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                     K230_SSI_SR_CMPLTD_DF_MASK) >>
                     K230_SSI_SR_CMPLTD_DF_SHIFT, ==, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_idma_quad_io_read(void)
{
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t actual[ARRAY_SIZE(expected)];

    configure_idma(qts, K230_SSI_TMOD_RO, 1, 6, FLASH_CMD_QUAD_IO,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(actual), K230_SSI_IDMAE | K230_SSI_AINC, true);
    qtest_memread(qts, K230_SSI_DMA_ADDR, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));
    assert_idma_stopped(qts, sizeof(actual));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_idma_quad_page_program(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t expected[256];
    uint8_t actual[sizeof(expected)];

    for (size_t i = 0; i < sizeof(expected); i++) {
        expected[i] = i ^ 0x5a;
    }
    qtest_memwrite(qts, K230_SSI_DMA_ADDR, expected, sizeof(expected));
    flash_write_enable(qts);
    configure_idma(qts, K230_SSI_TMOD_TO, 0, 0, FLASH_CMD_QUAD_PP,
                   K230_SSI_FLASH_PROGRAM_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(expected), K230_SSI_IDMAE | K230_SSI_AINC, false);
    assert_idma_stopped(qts, sizeof(expected));
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_DONECR),
                    ==, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR, 0);
    flash_wait_ready(qts);

    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PROGRAM_ADDR,
                   K230_SSI_DMA_ADDR + sizeof(expected), sizeof(actual),
                   K230_SSI_IDMAE | K230_SSI_AINC, false);
    qtest_memread(qts, K230_SSI_DMA_ADDR + sizeof(expected),
                  actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_idma_bad_address_and_axie(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR, 0);
    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, 0x100000000ULL, 4,
                   K230_SSI_IDMAE | K230_SSI_AINC, false);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_AXIE, ==, K230_SSI_INT_AXIE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_AXIE, ==, 0);
    assert_idma_stopped(qts, 0);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR,
                    K230_SSI_INT_AXIE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_AXIE, ==, K230_SSI_INT_AXIE);
    g_assert_true(k230_ssi_plic_pending(
        qts, k230_ssi_instances[0].first_irq + K230_SSI_IRQ_AXIE));
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIECR, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_AXIE, ==, K230_SSI_INT_AXIE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_AXIECR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_AXIE, ==, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_idma_dr_block_reset_and_unsupported(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t ctrlr0;

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR,
                    K230_SSI_IDMAE);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_write_frame(qts, K230_SPI0_BASE, 0xa5);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI0_BASE), ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_RO, 16, 3);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0) |
             (K230_SSI_FRF_QUAD << K230_SSI_CTRLR0_SPI_FRF_SHIFT);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR,
                    K230_SSI_IDMAE | K230_SSI_AINC);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    (K230_SSI_INT_DONE | K230_SSI_INT_AXIE), ==, 0);
    assert_idma_stopped(qts, 0);

    qtest_system_reset(qts);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    (K230_SSI_INT_DONE | K230_SSI_INT_AXIE), ==, 0);
    g_assert_cmpuint((k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                     K230_SSI_SR_CMPLTD_DF_MASK) >>
                     K230_SSI_SR_CMPLTD_DF_SHIFT, ==, 0);

    qtest_quit(qts);
}

static void test_hi_sys_ssi_ctrl_reset_and_mask(void)
{
    QTestState *qts = k230_ssi_start();

    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR),
                    ==, K230_SSI_CTRL_RESET);
    qtest_writel(qts, K230_SSI_CTRL_ADDR, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_IMPLEMENTED_MASK,
                    ==, (K230_SSI_CTRL_RESET &
                         ~K230_SSI_CTRL_WRITABLE_MASK) |
                        K230_SSI_CTRL_WRITABLE_MASK);

    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR),
                    ==, K230_SSI_CTRL_RESET);
    qtest_quit(qts);
}

static void test_hi_sys_mode_status_tracks_three_instances(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t ctrlr0;
    uint32_t expected_modes;

    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0,
                    (ctrlr0 & ~K230_SSI_CTRLR0_SPI_FRF_MASK) |
                    (K230_SSI_FRF_QUAD << K230_SSI_CTRLR0_SPI_FRF_SHIFT));
    ctrlr0 = k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_CTRLR0,
                    (ctrlr0 & ~K230_SSI_CTRLR0_SPI_FRF_MASK) |
                    (K230_SSI_FRF_DUAL << K230_SSI_CTRLR0_SPI_FRF_SHIFT));
    ctrlr0 = k230_ssi_readl(qts, K230_SPI2_BASE, K230_SSI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI2_BASE, K230_SSI_CTRLR0,
                    (ctrlr0 & ~K230_SSI_CTRLR0_SPI_FRF_MASK) |
                    (K230_SSI_FRF_OCTAL << K230_SSI_CTRLR0_SPI_FRF_SHIFT));

    expected_modes = (K230_SSI_FRF_QUAD <<
                      K230_SSI_CTRL_SPI0_MODE_SHIFT) |
                     (K230_SSI_FRF_DUAL <<
                      K230_SSI_CTRL_SPI1_MODE_SHIFT) |
                     (K230_SSI_FRF_OCTAL <<
                      K230_SSI_CTRL_SPI2_MODE_SHIFT);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    ((3U << K230_SSI_CTRL_SPI0_MODE_SHIFT) |
                     (3U << K230_SSI_CTRL_SPI1_MODE_SHIFT) |
                     (3U << K230_SSI_CTRL_SPI2_MODE_SHIFT)),
                    ==, expected_modes);

    qtest_quit(qts);
}

static void test_hi_sys_sleep_status_follows_enable_and_idle(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_SPI0_SLEEP, ==, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_SPI0_SLEEP,
                    ==, K230_SSI_CTRL_SPI0_SLEEP);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_SPI0_SLEEP, ==, 0);
    qtest_quit(qts);
}

static void test_hi_sys_sleep_status_after_idma(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR, 4,
                   K230_SSI_IDMAE | K230_SSI_AINC, false);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_SPI0_SLEEP,
                    ==, K230_SSI_CTRL_SPI0_SLEEP);

    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, 0x100000000ULL, 4,
                   K230_SSI_IDMAE | K230_SSI_AINC, false);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_SPI0_SLEEP,
                    ==, K230_SSI_CTRL_SPI0_SLEEP);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_ssi_ctrl_and_dr2_are_independent(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t before = qtest_readl(qts, K230_SSI_CTRL_ADDR);

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 32, 0);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_DR(2), 0x12345678);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_TXFLR),
                     ==, 1);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR), ==, before);

    qtest_quit(qts);
}

static void enable_xip(QTestState *qts)
{
    qtest_writel(qts, K230_SSI_CTRL_ADDR,
                 K230_SSI_CTRL_RESET | K230_SSI_CTRL_XIP_EN);
    g_assert_cmphex(qtest_readl(qts, K230_SSI_CTRL_ADDR) &
                    K230_SSI_CTRL_XIP_EN,
                    ==, K230_SSI_CTRL_XIP_EN);
}

static void configure_xip_read(QTestState *qts, uint8_t opcode,
                               unsigned int address_bits)
{
    uint32_t spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(0) |
                          K230_SSI_SPI_CTRLR0_ADDR_L(address_bits) |
                          K230_SSI_SPI_CTRLR0_INST_L_8 |
                          K230_SSI_SPI_CTRLR0_XIP_INST_EN;

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_INCR_INST, opcode);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
}

static void test_xip_enable_gate_and_read_only(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint64_t address = K230_FLASH_BASE + K230_SSI_FLASH_PATTERN_ADDR;

    g_assert_cmphex(qtest_readb(qts, address), ==, 0);
    configure_xip_read(qts, FLASH_CMD_READ, 24);
    enable_xip(qts);
    g_assert_cmphex(qtest_readb(qts, address), ==, 0xa5);

    qtest_writeb(qts, address, 0);
    g_assert_cmphex(qtest_readb(qts, address), ==, 0xa5);

    qtest_writel(qts, K230_SSI_CTRL_ADDR, K230_SSI_CTRL_RESET);
    g_assert_cmphex(qtest_readb(qts, address), ==, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_xip_read_widths(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint64_t address = K230_FLASH_BASE + K230_SSI_FLASH_PATTERN_ADDR;

    configure_xip_read(qts, FLASH_CMD_READ, 24);
    enable_xip(qts);
    g_assert_cmphex(qtest_readb(qts, address), ==, 0xa5);
    g_assert_cmphex(qtest_readw(qts, address), ==, 0x5aa5);
    g_assert_cmphex(qtest_readl(qts, address), ==, 0xc33c5aa5);
    g_assert_cmphex(qtest_readq(qts, address),
                    ==, 0x44332211c33c5aa5ULL);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_xip_address_width_comes_from_spi_ctrlr0(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t spi_ctrlr0;

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_INCR_INST,
                    FLASH_CMD_READ4);
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(0) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(32) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_XIP_INST_EN;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    enable_xip(qts);

    g_assert_cmphex(qtest_readl(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_HIGH_ADDR), ==, 0x74737271);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_xip_dummy_and_mode_bits_come_from_registers(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= K230_SSI_FRF_QUAD << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_INCR_INST,
                    FLASH_CMD_QUAD_IO);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_MODE_BITS, 0xff);
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(1) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_XIP_INST_EN |
                 K230_SSI_SPI_CTRLR0_XIP_MD_EN |
                 K230_SSI_SPI_CTRLR0_XIP_MBL_8 |
                 K230_SSI_SPI_CTRLR0_WAIT(4);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    enable_xip(qts);

    g_assert_cmphex(qtest_readl(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR),
                    ==, 0xc33c5aa5);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_pio_and_xip_share_flash_without_stale_cs(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t command = FLASH_CMD_JEDEC;
    uint8_t id[3];

    configure_xip_read(qts, FLASH_CMD_READ, 24);
    enable_xip(qts);
    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0xa5);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_enable_cs(qts, K230_SPI0_BASE, BIT(0));
    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0xa5);

    flash_read_transaction(qts, &command, 1, id, sizeof(id));
    g_assert_cmphex(id[0], ==, 0xef);
    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0xa5);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_register_contract(void)
{
    test_reset_values();
    test_profile_reset_values();
    test_register_write_masks();
    test_ser_masks_follow_sdk_num_cs();
    test_read_only_and_reserved_registers();
    test_enabled_write_contract();
    test_internal_dma_registers_are_passive();
    test_system_reset_restores_contract();
}

static void test_pio_data_path(void)
{
    test_dr_aliases_share_one_fifo();
    test_dr_write_while_disabled_is_ignored();
    test_fifo_depth_is_256_frames();
    test_dfs_masks_4_8_16_32_bit_frames();
    test_tmod_transmit_and_receive();
    test_tmod_transmit_only_discards_rx();
    test_tmod_transmit_only_refills_in_batches();
    test_ser_deselect_aborts_transmit_only();
    test_tmod_receive_only_uses_ndf();
    test_tmod_eeprom_read_has_separate_rx_count();
    test_disable_stops_transfer_and_clears_fifos();
    test_dynamic_status_during_paused_rx_only();
}

static void test_interrupt_controller(void)
{
    test_watermark_and_mask_relationship();
    test_rx_underflow_latches_and_read_clears();
    test_tx_overflow_latches_and_txeicr_clears();
    test_rx_overflow_latches_and_rxoicr_clears();
    test_icr_clear_scope();
    test_reset_clears_latched_causes();
    test_mst_txu_done_axie_inactive_without_causes();
}

static void test_plic_routing(void)
{
    test_plic_txe_reset_routing();
    test_plic_rxu_routing_and_instance_isolation();
    test_plic_rxf_routing();
    test_plic_txo_routing();
    test_plic_rxo_routing();
}

static void test_qspi_config(void)
{
    test_unsupported_enhanced_configs_do_not_transfer();
    test_supported_enhanced_command_is_parsed();
}

static void test_spi_nor(void)
{
    test_jedec_id();
    test_flash_read_3byte_address();
    test_flash_read_4byte_address();
    test_page_program_and_readback();
    test_native_cs_full_page_program();
    test_sector_erase_and_readback();
    test_chip_select_restarts_command();
    test_failed_enhanced_reset_falls_back_to_read_id();
}

static void test_qspi_sdr(void)
{
    test_dual_and_quad_output_read();
    test_quad_io_mode_bits_and_dummy();
    test_quad_page_program_streaming();
    test_enhanced_rx_fifo_resume();
    test_enhanced_prefix_is_atomic();
}

static void test_idma(void)
{
    test_idma_quad_output_read_and_done();
    test_idma_quad_io_read();
    test_idma_quad_page_program();
    test_idma_bad_address_and_axie();
    test_idma_dr_block_reset_and_unsupported();
}

static void test_hi_sys(void)
{
    test_hi_sys_ssi_ctrl_reset_and_mask();
    test_hi_sys_mode_status_tracks_three_instances();
    test_hi_sys_sleep_status_follows_enable_and_idle();
    test_hi_sys_sleep_status_after_idma();
    test_ssi_ctrl_and_dr2_are_independent();
}

static void test_xip_read_window(void)
{
    test_xip_enable_gate_and_read_only();
    test_xip_read_widths();
    test_xip_address_width_comes_from_spi_ctrlr0();
    test_xip_dummy_and_mode_bits_come_from_registers();
    test_pio_and_xip_share_flash_without_stale_cs();
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/k230-dw-ssi/register-contract", test_register_contract);
    qtest_add_func("/k230-dw-ssi/pio-data-path", test_pio_data_path);
    qtest_add_func("/k230-dw-ssi/interrupt-controller",
                   test_interrupt_controller);
    qtest_add_func("/k230-dw-ssi/plic-routing", test_plic_routing);
    qtest_add_func("/k230-dw-ssi/qspi-config", test_qspi_config);
    qtest_add_func("/k230-dw-ssi/spi-nor", test_spi_nor);
    qtest_add_func("/k230-dw-ssi/qspi-sdr", test_qspi_sdr);
    qtest_add_func("/k230-dw-ssi/idma", test_idma);
    qtest_add_func("/k230-dw-ssi/hi-sys", test_hi_sys);
    qtest_add_func("/k230-dw-ssi/xip-read-window", test_xip_read_window);
    return g_test_run();
}
