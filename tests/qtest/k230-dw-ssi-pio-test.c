/*
 * K230 DWC SSI FIFO、PIO 与动态状态 qtest
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

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
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXO, ==, K230_SSI_INT_TXO);

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

static void test_tmod_receive_only_uses_ndf(void)
{
    QTestState *qts = k230_ssi_start();

    configure_loopback(qts, K230_SSI_TMOD_RO, 8, 3);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
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
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0x03);
    k230_ssi_write_frame(qts, K230_SPI1_BASE, 0x00);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 1);
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

static void test_dynamic_watermark_status(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI1_BASE, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_writel(qts, K230_SPI1_BASE, K230_SSI_TXFTLR, 3);
    k230_ssi_enable_cs(qts, K230_SPI1_BASE, 0);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, K230_SSI_INT_TXE);
    for (int i = 0; i < 4; i++) {
        k230_ssi_write_frame(qts, K230_SPI1_BASE, i);
    }
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI1_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_TXE, ==, 0);

    qtest_quit(qts);
}

void k230_ssi_register_pio_tests(void)
{
    qtest_add_func("/k230-dw-ssi/pio/dr-aliases",
                   test_dr_aliases_share_one_fifo);
    qtest_add_func("/k230-dw-ssi/pio/fifo-depth-256",
                   test_fifo_depth_is_256_frames);
    qtest_add_func("/k230-dw-ssi/pio/dfs-frame-mask",
                   test_dfs_masks_4_8_16_32_bit_frames);
    qtest_add_func("/k230-dw-ssi/pio/tmod-tr",
                   test_tmod_transmit_and_receive);
    qtest_add_func("/k230-dw-ssi/pio/tmod-to",
                   test_tmod_transmit_only_discards_rx);
    qtest_add_func("/k230-dw-ssi/pio/tmod-ro",
                   test_tmod_receive_only_uses_ndf);
    qtest_add_func("/k230-dw-ssi/pio/tmod-eeprom-read",
                   test_tmod_eeprom_read_has_separate_rx_count);
    qtest_add_func("/k230-dw-ssi/pio/disable-clears-fifo",
                   test_disable_stops_transfer_and_clears_fifos);
    qtest_add_func("/k230-dw-ssi/pio/dynamic-watermark",
                   test_dynamic_watermark_status);
}

