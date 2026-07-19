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
#define K230_SSI_SPI_CTRLR0_XIP_INST_EN   BIT(20)
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







const K230SsiInstance k230_ssi_instances[3] = {
    {
        .name = "spi0",
        .base = K230_SPI0_BASE,
        .num_cs = 1,
        .max_lines = 8,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_FMC_RESET,
        .first_irq = 146,
        .has_xip = true,
    }, {
        .name = "spi1",
        .base = K230_SPI1_BASE,
        .num_cs = 5,
        .max_lines = 4,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_SPI_RESET,
        .first_irq = 155,
        .has_xip = false,
    }, {
        .name = "spi2",
        .base = K230_SPI2_BASE,
        .num_cs = 5,
        .max_lines = 4,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_SPI_RESET,
        .first_irq = 164,
        .has_xip = false,
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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/k230-dw-ssi/register-contract", test_register_contract);
    return g_test_run();
}
