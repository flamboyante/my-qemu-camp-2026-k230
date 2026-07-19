/*
 * K230 HI_SYS SSI_CTRL qtest
 *
 * Patch 8 RED scaffold.  SSI_CTRL belongs to HI_SYS_CONFIG at
 * 0x91585068; every SSI controller's base + 0x068 remains DR2.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

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

    expected_modes = (K230_SSI_FRF_QUAD <<
                      K230_SSI_CTRL_SPI0_MODE_SHIFT) |
                     (K230_SSI_FRF_DUAL <<
                      K230_SSI_CTRL_SPI1_MODE_SHIFT);
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

    /* TRM 只要求禁用且空闲后延迟置位，不写死真机延迟。 */
    k230_ssi_wait_mask(qts, K230_HI_SYS_BASE, 0x68,
                       K230_SSI_CTRL_SPI0_SLEEP,
                       K230_SSI_CTRL_SPI0_SLEEP);

    qtest_quit(qts);
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

void k230_ssi_register_hi_sys_tests(void)
{
    qtest_add_func("/k230-dw-ssi/hi-sys/reset-mask",
                   test_hi_sys_ssi_ctrl_reset_and_mask);
    qtest_add_func("/k230-dw-ssi/hi-sys/mode-status",
                   test_hi_sys_mode_status_tracks_three_instances);
    qtest_add_func("/k230-dw-ssi/hi-sys/sleep-status",
                   test_hi_sys_sleep_status_follows_enable_and_idle);
    qtest_add_func("/k230-dw-ssi/hi-sys/dr2-independent",
                   test_ssi_ctrl_and_dr2_are_independent);
}
