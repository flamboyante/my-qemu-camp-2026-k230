/*
 * K230 DWC SSI 控制器内部中断 qtest
 *
 * 外部九路顺序固定为 TXE/TXO/RXF/RXO/TXU/RXU/MST/DONE/AXIE；
 * 本文件只验证 RISR/ISR/IMR、水位、锁存和 RC，不读取 PLIC。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

static void test_watermark_and_mask_relationship(void)
{
    QTestState *qts = k230_ssi_start();
    uint32_t ctrlr0;

    /* TXE：空 FIFO active，超过 TFT 后撤销，pump 排空后恢复。 */
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

    /* RXF：RFT=0 时第一个接收 frame 拉高，读空后自动撤销。 */
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
    /* SER=0 保持 deferred 状态，避免同步 pump 消费 TX FIFO。 */
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

    /*
     * 空读产生 RXU。ICR 应清 RXU，
     * 但不能伪造或清除 DONE/AXIE。
     */
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

void k230_ssi_register_irq_tests(void)
{
    qtest_add_func("/k230-dw-ssi/irq/watermark-mask",
                   test_watermark_and_mask_relationship);
    qtest_add_func("/k230-dw-ssi/irq/rxu-read-clear",
                   test_rx_underflow_latches_and_read_clears);
    qtest_add_func("/k230-dw-ssi/irq/txo-read-clear",
                   test_tx_overflow_latches_and_txeicr_clears);
    qtest_add_func("/k230-dw-ssi/irq/rxo-read-clear",
                   test_rx_overflow_latches_and_rxoicr_clears);
    qtest_add_func("/k230-dw-ssi/irq/icr-clear-scope",
                   test_icr_clear_scope);
    qtest_add_func("/k230-dw-ssi/irq/inactive-causes",
                   test_mst_txu_done_axie_inactive_without_causes);
}
