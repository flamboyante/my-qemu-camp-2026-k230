/*
 * K230 DWC SSI PLIC 路由 qtest
 *
 * Patch 7 RED 脚手架：路由表已固定，完成 27 路接线后这些
 * 用例转为 GREEN。
 * 外部九路顺序固定为 TXE/TXO/RXF/RXO/TXU/RXU/MST/DONE/AXIE。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

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

void k230_ssi_register_plic_tests(void)
{
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
