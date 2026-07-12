/*
 * QTest testcase for the K230 DWC SSI compatible SPI/QSPI controller.
 *
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "libqtest.h"

#define K230_QSPI0_BASE 0x91582000
#define K230_QSPI1_BASE 0x91583000
#define K230_SPI_BASE   0x91584000
#define K230_FLASH_BASE 0xc0000000

#define K230_DW_SSI_CTRLR0          0x000
#define K230_DW_SSI_CTRLR1          0x004
#define K230_DW_SSI_SSIENR          0x008
#define K230_DW_SSI_SER             0x010
#define K230_DW_SSI_BAUDR           0x014
#define K230_DW_SSI_TXFTLR          0x018
#define K230_DW_SSI_RXFTLR          0x01c
#define K230_DW_SSI_TXFLR           0x020
#define K230_DW_SSI_RXFLR           0x024
#define K230_DW_SSI_SR              0x028
#define K230_DW_SSI_IMR             0x02c
#define K230_DW_SSI_ISR             0x030
#define K230_DW_SSI_RISR            0x034
#define K230_DW_SSI_DR0             0x060
#define K230_DW_SSI_SPI_CTRLR0      0x0f4
#define K230_DW_SSI_XIP_INCR_INST   0x100
#define K230_DW_SSI_SSIC_VERSION_ID 0x05c

#define K230_DW_SSI_CTRLR0_RESET    0x00004007
#define K230_DW_SSI_VERSION         0x3130332a

#define K230_DW_SSI_SR_TFE          BIT(2)
#define K230_DW_SSI_SR_RFNE         BIT(3)
#define K230_DW_SSI_SR_TFNF         BIT(1)
#define K230_DW_SSI_INT_RXFI        BIT(4)

static void write_dr(QTestState *qts, uint64_t base, uint8_t value)
{
    qtest_writel(qts, base + K230_DW_SSI_DR0, value);
}

static uint8_t read_dr(QTestState *qts, uint64_t base)
{
    return qtest_readl(qts, base + K230_DW_SSI_DR0) & 0xff;
}

static void test_reset_values(void)
{
    static const uint64_t bases[] = {
        K230_QSPI0_BASE,
        K230_QSPI1_BASE,
        K230_SPI_BASE,
    };
    QTestState *qts = qtest_init("-machine k230");

    for (int i = 0; i < ARRAY_SIZE(bases); i++) {
        uint64_t base = bases[i];

        g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_CTRLR0), ==,
                        K230_DW_SSI_CTRLR0_RESET);
        g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_SSIENR), ==, 0);
        g_assert_cmphex(qtest_readl(qts,
                                    base + K230_DW_SSI_SSIC_VERSION_ID), ==,
                        K230_DW_SSI_VERSION);
        g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_SR) &
                        (K230_DW_SSI_SR_TFE | K230_DW_SSI_SR_TFNF), ==,
                        K230_DW_SSI_SR_TFE | K230_DW_SSI_SR_TFNF);
    }

    qtest_quit(qts);
}

static void test_register_readback(void)
{
    QTestState *qts = qtest_init("-machine k230");
    uint64_t base = K230_QSPI0_BASE;

    qtest_writel(qts, base + K230_DW_SSI_CTRLR1, 0x1234);
    qtest_writel(qts, base + K230_DW_SSI_SER, 0x1);
    qtest_writel(qts, base + K230_DW_SSI_BAUDR, 0x20);
    qtest_writel(qts, base + K230_DW_SSI_TXFTLR, 0x3);
    qtest_writel(qts, base + K230_DW_SSI_RXFTLR, 0x4);
    qtest_writel(qts, base + K230_DW_SSI_IMR, 0x1f);
    qtest_writel(qts, base + K230_DW_SSI_SPI_CTRLR0, 0x0300000b);
    qtest_writel(qts, base + K230_DW_SSI_XIP_INCR_INST, 0xabcd000b);

    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_CTRLR1), ==, 0x1234);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_SER), ==, 0x1);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_BAUDR), ==, 0x20);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_TXFTLR), ==, 0x3);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_RXFTLR), ==, 0x4);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_IMR), ==, 0x1f);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_SPI_CTRLR0), ==,
                    0x0300000b);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_XIP_INCR_INST), ==,
                    0x0b);

    qtest_quit(qts);
}

static char *create_flash_image(void)
{
    char *tmp_path = NULL;
    int fd;
    int ret;
    uint8_t pattern[] = {
        0xa5, 0x5a, 0x3c, 0xc3, 0x11, 0x22, 0x33, 0x44,
    };
    uint8_t high_pattern[] = {
        0x71, 0x72, 0x73, 0x74,
    };

    fd = g_file_open_tmp("qtest.k230.w25q256.XXXXXX", &tmp_path, NULL);
    g_assert(fd >= 0);

    ret = ftruncate(fd, 32 * 1024 * 1024);
    g_assert_cmpint(ret, ==, 0);
    ret = pwrite(fd, pattern, sizeof(pattern), 0x100);
    g_assert_cmpint(ret, ==, sizeof(pattern));
    ret = pwrite(fd, high_pattern, sizeof(high_pattern), 0x1000100);
    g_assert_cmpint(ret, ==, sizeof(high_pattern));

    close(fd);
    return tmp_path;
}

static void test_jedec_id(void)
{
    g_autofree char *tmp_path = create_flash_image();
    QTestState *qts = qtest_initf("-machine k230 "
                                  "-drive file=%s,format=raw,if=mtd",
                                  tmp_path);
    uint64_t base = K230_SPI_BASE;
    uint8_t id[3];

    qtest_writel(qts, base + K230_DW_SSI_SER, 1);
    qtest_writel(qts, base + K230_DW_SSI_SSIENR, 1);

    write_dr(qts, base, 0x9f);
    write_dr(qts, base, 0);
    write_dr(qts, base, 0);
    write_dr(qts, base, 0);

    g_assert_cmpuint(qtest_readl(qts, base + K230_DW_SSI_RXFLR), >=, 4);
    (void)read_dr(qts, base);
    id[0] = read_dr(qts, base);
    id[1] = read_dr(qts, base);
    id[2] = read_dr(qts, base);

    g_assert_cmphex(id[0], ==, 0xef);
    g_assert_cmphex(id[1], ==, 0x40);
    g_assert_cmphex(id[2], ==, 0x19);

    qtest_writel(qts, base + K230_DW_SSI_SSIENR, 0);
    qtest_quit(qts);
    unlink(tmp_path);
}

static void test_disabled_dr_write_does_not_stale_fifo(void)
{
    QTestState *qts = qtest_init("-machine k230");
    uint64_t base = K230_SPI_BASE;

    write_dr(qts, base, 0xff);

    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_TXFLR), ==, 0);
    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_SR) &
                    (K230_DW_SSI_SR_TFE | K230_DW_SSI_SR_TFNF), ==,
                    K230_DW_SSI_SR_TFE | K230_DW_SSI_SR_TFNF);

    qtest_writel(qts, base + K230_DW_SSI_SER, 1);
    qtest_writel(qts, base + K230_DW_SSI_SSIENR, 1);

    g_assert_cmphex(qtest_readl(qts, base + K230_DW_SSI_TXFLR), ==, 0);
    write_dr(qts, base, 0x9f);
    write_dr(qts, base, 0);
    write_dr(qts, base, 0);
    write_dr(qts, base, 0);

    g_assert_cmpuint(qtest_readl(qts, base + K230_DW_SSI_RXFLR), >=, 4);

    qtest_quit(qts);
}

static void test_internal_interrupt_status(void)
{
    QTestState *qts = qtest_init("-machine k230");
    uint64_t base = K230_SPI_BASE;

    qtest_writel(qts, base + K230_DW_SSI_RXFTLR, 0);
    qtest_writel(qts, base + K230_DW_SSI_IMR, K230_DW_SSI_INT_RXFI);
    g_assert((qtest_readl(qts, base + K230_DW_SSI_RISR) &
              K230_DW_SSI_INT_RXFI) == 0);
    g_assert((qtest_readl(qts, base + K230_DW_SSI_ISR) &
              K230_DW_SSI_INT_RXFI) == 0);

    qtest_writel(qts, base + K230_DW_SSI_SER, 1);
    qtest_writel(qts, base + K230_DW_SSI_SSIENR, 1);
    write_dr(qts, base, 0x9f);

    g_assert((qtest_readl(qts, base + K230_DW_SSI_RISR) &
              K230_DW_SSI_INT_RXFI) == K230_DW_SSI_INT_RXFI);
    g_assert((qtest_readl(qts, base + K230_DW_SSI_ISR) &
              K230_DW_SSI_INT_RXFI) == K230_DW_SSI_INT_RXFI);

    (void)read_dr(qts, base);
    g_assert((qtest_readl(qts, base + K230_DW_SSI_RISR) &
              K230_DW_SSI_INT_RXFI) == 0);

    qtest_quit(qts);
}

static void test_xip_window_read(void)
{
    g_autofree char *tmp_path = create_flash_image();
    QTestState *qts = qtest_initf("-machine k230 "
                                  "-drive file=%s,format=raw,if=mtd",
                                  tmp_path);

    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE + 0x100), ==, 0xa5);
    g_assert_cmphex(qtest_readw(qts, K230_FLASH_BASE + 0x100), ==, 0x5aa5);
    g_assert_cmphex(qtest_readl(qts, K230_FLASH_BASE + 0x100), ==, 0xc33c5aa5);
    g_assert_cmphex(qtest_readq(qts, K230_FLASH_BASE + 0x100), ==,
                    0x44332211c33c5aa5ULL);

    qtest_writel(qts, K230_SPI_BASE + K230_DW_SSI_XIP_INCR_INST, 0x13);
    g_assert_cmphex(qtest_readl(qts, K230_FLASH_BASE + 0x1000100), ==,
                    0x74737271);

    qtest_writeb(qts, K230_FLASH_BASE + 0x100, 0);
    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE + 0x100), ==, 0xa5);

    qtest_quit(qts);
    unlink(tmp_path);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/k230-dw-ssi/reset_values", test_reset_values);
    qtest_add_func("/k230-dw-ssi/register_readback", test_register_readback);
    qtest_add_func("/k230-dw-ssi/jedec_id", test_jedec_id);
    qtest_add_func("/k230-dw-ssi/disabled_dr_write_does_not_stale_fifo",
                   test_disabled_dr_write_does_not_stale_fifo);
    qtest_add_func("/k230-dw-ssi/internal_interrupt_status",
                   test_internal_interrupt_status);
    qtest_add_func("/k230-dw-ssi/xip_window_read", test_xip_window_read);

    return g_test_run();
}
