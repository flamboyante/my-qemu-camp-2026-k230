/*
 * K230 DWC SSI 寄存器与实例 profile qtest
 *
 * 这些断言固定 SDK/TRM 软件可见契约。
 * 当前原型若与断言不同，
 * 应修改设备模型或重新核对证据，
 * 不能为了让旧实现通过而放宽测试。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

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
        /* 避免设置 IDMAE；本用例只验证 enabled 状态下寄存器可编程。 */
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

    /*
     * SSIENR=1 不等于事务 active。TRM 未明确锁定的寄存器由 guest
     * 软件保证更新时序，设备模型仍接受写入并执行各自的字段掩码。
     */
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

void k230_ssi_register_reg_tests(void)
{
    qtest_add_func("/k230-dw-ssi/reg/reset-values", test_reset_values);
    qtest_add_func("/k230-dw-ssi/reg/profile-reset-values",
                   test_profile_reset_values);
    qtest_add_func("/k230-dw-ssi/reg/write-masks",
                   test_register_write_masks);
    qtest_add_func("/k230-dw-ssi/reg/ser-num-cs",
                   test_ser_masks_follow_sdk_num_cs);
    qtest_add_func("/k230-dw-ssi/reg/read-only-and-razwi",
                   test_read_only_and_reserved_registers);
    qtest_add_func("/k230-dw-ssi/reg/enabled-write-contract",
                   test_enabled_write_contract);
    qtest_add_func("/k230-dw-ssi/reg/internal-dma-passive",
                   test_internal_dma_registers_are_passive);
    qtest_add_func("/k230-dw-ssi/reg/system-reset",
                   test_system_reset_restores_contract);
}
