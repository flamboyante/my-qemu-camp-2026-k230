/*
 * K230 spi0 XIP window qtest
 *
 * Patch 9 使用 HI_SYS.SSI_CTRL.ssi0_xip_en 作为窗口门控。
 * XIP 只属于 SDK 逻辑 spi0@0x91584000。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

#define FLASH_CMD_JEDEC     0x9f
#define FLASH_CMD_READ      0x03
#define FLASH_CMD_FAST_READ 0x0b
#define FLASH_CMD_READ4     0x13
#define FLASH_CMD_QUAD_IO   0xeb
#define K230_SSI_SPI_CTRLR0_XIP_MBL_8 (2U << 26)

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

static void test_xip_enable_gate(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0);
    configure_xip_read(qts, FLASH_CMD_READ, 24);
    enable_xip(qts);
    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0xa5);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_xip_read_widths(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint64_t addr = K230_FLASH_BASE + K230_SSI_FLASH_PATTERN_ADDR;

    configure_xip_read(qts, FLASH_CMD_READ, 24);
    enable_xip(qts);
    g_assert_cmphex(qtest_readb(qts, addr), ==, 0xa5);
    g_assert_cmphex(qtest_readw(qts, addr), ==, 0x5aa5);
    g_assert_cmphex(qtest_readl(qts, addr), ==, 0xc33c5aa5);
    g_assert_cmphex(qtest_readq(qts, addr), ==, 0x44332211c33c5aa5ULL);

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
    uint32_t spi_ctrlr0;

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
    uint8_t tx[] = { FLASH_CMD_JEDEC, 0 };
    uint8_t rx[ARRAY_SIZE(tx)];

    configure_xip_read(qts, FLASH_CMD_READ, 24);
    enable_xip(qts);
    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0xa5);

    k230_ssi_standard_transaction(qts, K230_SPI0_BASE,
                                  tx, rx, ARRAY_SIZE(tx));
    g_assert_cmphex(rx[1], ==, 0xef);

    g_assert_cmphex(qtest_readb(qts, K230_FLASH_BASE +
                                K230_SSI_FLASH_PATTERN_ADDR), ==, 0xa5);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

void k230_ssi_register_xip_tests(void)
{
    qtest_add_func("/k230-dw-ssi/xip/enable-gate", test_xip_enable_gate);
    qtest_add_func("/k230-dw-ssi/xip/read-widths", test_xip_read_widths);
    qtest_add_func("/k230-dw-ssi/xip/address-width",
                   test_xip_address_width_comes_from_spi_ctrlr0);
    qtest_add_func("/k230-dw-ssi/xip/mode-bits-dummy",
                   test_xip_dummy_and_mode_bits_come_from_registers);
    qtest_add_func("/k230-dw-ssi/xip/pio-coordination",
                   test_pio_and_xip_share_flash_without_stale_cs);
}
