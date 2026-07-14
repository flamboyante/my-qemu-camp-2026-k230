/*
 * K230 DWC SSI 内部中断与 PLIC 路由 qtest
 *
 * 外部九路顺序固定为 TXE/TXO/RXF/RXO/TXU/RXU/MST/DONE/AXIE；
 * RISR 位号与外部 GPIO 顺序并不相同，测试中必须显式映射。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

static void test_watermark_and_mask_relationship(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_disable(qts, K230_SPI1_BASE);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_TXFTLR, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_IMR, 0);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_TXE, ==, 0);

    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_IMR,
                    K230_SSI_INT_TXE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);

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
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);
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

static void test_plic_txe_reset_routing(void)
{
    QTestState *qts = k230_ssi_start();

    /*
     * IMR 复位为 0x3f，空 TX FIFO 满足 TXE 水位，
     * 三路 TXE 应到达各自起始号。
     */
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
                qts, k230_ssi_instances[other].first_irq + K230_SSI_IRQ_RXU));
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
        k230_ssi_enable_cs(qts, inst->base, 1);
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

        k230_ssi_writel(qts, inst->base, K230_SSI_IMR, K230_SSI_INT_TXO);
        k230_ssi_configure(qts, inst->base, K230_SSI_TMOD_TR, 32, 0);
        k230_ssi_enable_cs(qts, inst->base, 0);
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
        k230_ssi_enable_cs(qts, inst->base, 1);
        for (int frame = 0; frame <= K230_SSI_FIFO_DEPTH; frame++) {
            k230_ssi_write_frame(qts, inst->base, frame);
        }

        g_assert_true(k230_ssi_plic_pending(qts,
                                           inst->first_irq +
                                           K230_SSI_IRQ_RXO));
        qtest_quit(qts);
    }
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
    qtest_add_func("/k230-dw-ssi/irq/plic-txe-reset-routing",
                   test_plic_txe_reset_routing);
    qtest_add_func("/k230-dw-ssi/irq/plic-rxu-isolation",
                   test_plic_rxu_routing_and_instance_isolation);
    qtest_add_func("/k230-dw-ssi/irq/plic-rxf-routing",
                   test_plic_rxf_routing);
    qtest_add_func("/k230-dw-ssi/irq/plic-txo-routing",
                   test_plic_txo_routing);
    qtest_add_func("/k230-dw-ssi/irq/plic-rxo-routing",
                   test_plic_rxo_routing);
}
