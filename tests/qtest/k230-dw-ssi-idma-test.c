/*
 * K230 DWC SSI internal DMA qtest
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

#define FLASH_CMD_WREN      0x06
#define FLASH_CMD_QUAD_OUT  0x6b
#define FLASH_CMD_QUAD_IO   0xeb
#define FLASH_CMD_QUAD_PP   0x32

#define K230_SSI_IDMAE      BIT(2)
#define K230_SSI_AINC       BIT(6)
#define K230_SSI_DMA_ADDR   0x80201000ULL

static void configure_idma(QTestState *qts, uint32_t tmod,
                           uint32_t trans_type, uint32_t wait_cycles,
                           uint8_t opcode, uint32_t flash_address,
                           uint64_t dma_address, size_t length)
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
                 K230_SSI_SPI_CTRLR0_INST_L_8;
    spi_ctrlr0 |= K230_SSI_SPI_CTRLR0_WAIT(wait_cycles);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_IMR, K230_SSI_INT_DONE);

    /* Program IDMA first to verify that register write order is irrelevant. */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR,
                    K230_SSI_IDMAE | K230_SSI_AINC);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPIDR, opcode);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPIAR, flash_address);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIAR0, dma_address);
    if (dma_address >> 32) {
        k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_AXIAR1,
                        dma_address >> 32);
    }
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
}

static void assert_idma_done(QTestState *qts)
{
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, K230_SSI_INT_DONE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_DONE, ==, K230_SSI_INT_DONE);
    g_assert_true(k230_ssi_plic_pending(qts,
                                        k230_ssi_instances[0].first_irq +
                                        K230_SSI_IRQ_DONE));
}

static void test_quad_idma_read_and_done(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    qtest_memset(qts, K230_SSI_DMA_ADDR, 0, sizeof(actual));
    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(actual));
    assert_idma_done(qts);
    qtest_memread(qts, K230_SSI_DMA_ADDR, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_DONECR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_DONE, ==, 0);
    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_donecr_write_is_ignored(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    qtest_memset(qts, K230_SSI_DMA_ADDR, 0, 4);
    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR, 4);
    assert_idma_done(qts);

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DONECR, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, K230_SSI_INT_DONE);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_DONECR),
                    ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_idma_page_program(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t expected[256];
    uint8_t actual[sizeof(expected)];
    uint8_t wren = FLASH_CMD_WREN;

    for (size_t i = 0; i < sizeof(expected); i++) {
        expected[i] = i;
    }
    qtest_memwrite(qts, K230_SSI_DMA_ADDR, expected, sizeof(expected));
    k230_ssi_standard_transaction(qts, K230_SPI0_BASE, &wren, NULL, 1);

    configure_idma(qts, K230_SSI_TMOD_TO, 0, 0, FLASH_CMD_QUAD_PP,
                   K230_SSI_FLASH_PROGRAM_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(expected));
    assert_idma_done(qts);
    qtest_clock_step(qts, 10000000);

    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PROGRAM_ADDR,
                   K230_SSI_DMA_ADDR + sizeof(expected), sizeof(actual));
    assert_idma_done(qts);
    qtest_memread(qts, K230_SSI_DMA_ADDR + sizeof(expected), actual,
                  sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_io_idma_read(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    configure_idma(qts, K230_SSI_TMOD_RO, 1, 6, FLASH_CMD_QUAD_IO,
                   K230_SSI_FLASH_PATTERN_ADDR, K230_SSI_DMA_ADDR,
                   sizeof(actual));
    assert_idma_done(qts);
    qtest_memread(qts, K230_SSI_DMA_ADDR, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_idma_bad_address_does_not_complete(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    configure_idma(qts, K230_SSI_TMOD_RO, 0, 8, FLASH_CMD_QUAD_OUT,
                   K230_SSI_FLASH_PATTERN_ADDR, 0x100000000ULL, 4);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_DONE, ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_ISR) &
                    K230_SSI_INT_DONE, ==, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_idma_dr_access_does_not_use_fifo(void)
{
    QTestState *qts = k230_ssi_start();

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DMACR,
                    K230_SSI_IDMAE);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_DR0, 0xa5);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 0);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);
    (void)k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_DR0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RISR) &
                    K230_SSI_INT_RXU, ==, 0);

    qtest_quit(qts);
}

void k230_ssi_register_idma_tests(void)
{
    qtest_add_func("/k230-dw-ssi/idma/quad-read-done",
                   test_quad_idma_read_and_done);
    qtest_add_func("/k230-dw-ssi/idma/donecr-write-ignored",
                   test_donecr_write_is_ignored);
    qtest_add_func("/k230-dw-ssi/idma/quad-page-program",
                   test_quad_idma_page_program);
    qtest_add_func("/k230-dw-ssi/idma/quad-io-read",
                   test_quad_io_idma_read);
    qtest_add_func("/k230-dw-ssi/idma/bad-address",
                   test_idma_bad_address_does_not_complete);
    qtest_add_func("/k230-dw-ssi/idma/dr-access-blocked",
                   test_idma_dr_access_does_not_use_fifo);
}
