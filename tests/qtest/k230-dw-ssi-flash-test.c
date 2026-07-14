/*
 * K230 spi0、W25Q256 与增强 QSPI qtest
 *
 * Flash 命令通过真实 m25p80 后端执行。
 * 测试关注控制器组织出的指令、地址、dummy 和数据阶段，
 * 不把 QEMU SSIBus 当成逐线波形模型。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

#define FLASH_CMD_WREN          0x06
#define FLASH_CMD_RDSR          0x05
#define FLASH_CMD_READ          0x03
#define FLASH_CMD_READ4         0x13
#define FLASH_CMD_FAST_READ     0x0b
#define FLASH_CMD_QUAD_OUT      0x6b
#define FLASH_CMD_QUAD_IO       0xeb
#define FLASH_CMD_PP            0x02
#define FLASH_CMD_SE            0x20
#define FLASH_CMD_JEDEC         0x9f
#define FLASH_SR_WIP            BIT(0)

static void flash_transaction(QTestState *qts, const uint8_t *tx,
                              uint8_t *rx, size_t len)
{
    k230_ssi_standard_transaction(qts, K230_SPI0_BASE, tx, rx, len);
}

static void flash_read(QTestState *qts, uint8_t opcode, uint32_t address,
                       unsigned int addr_bytes, unsigned int dummy_bytes,
                       uint8_t *data, size_t len)
{
    size_t prefix = 1 + addr_bytes + dummy_bytes;
    g_autofree uint8_t *tx = g_new0(uint8_t, prefix + len);
    g_autofree uint8_t *rx = g_new0(uint8_t, prefix + len);

    tx[0] = opcode;
    for (unsigned int i = 0; i < addr_bytes; i++) {
        tx[1 + i] = address >> (8 * (addr_bytes - i - 1));
    }
    flash_transaction(qts, tx, rx, prefix + len);
    memcpy(data, rx + prefix, len);
}

static uint8_t flash_read_status(QTestState *qts)
{
    uint8_t tx[] = { FLASH_CMD_RDSR, 0 };
    uint8_t rx[ARRAY_SIZE(tx)];

    flash_transaction(qts, tx, rx, ARRAY_SIZE(tx));
    return rx[1];
}

static void flash_wait_ready(QTestState *qts)
{
    for (int i = 0; i < 1000; i++) {
        if (!(flash_read_status(qts) & FLASH_SR_WIP)) {
            return;
        }
        qtest_clock_step(qts, 1000000);
    }

    g_assert_cmphex(flash_read_status(qts) & FLASH_SR_WIP, ==, 0);
}

static void flash_write_enable(QTestState *qts)
{
    uint8_t cmd = FLASH_CMD_WREN;

    flash_transaction(qts, &cmd, NULL, 1);
}

static void test_jedec_id(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t tx[] = { FLASH_CMD_JEDEC, 0, 0, 0 };
    uint8_t rx[ARRAY_SIZE(tx)];

    flash_transaction(qts, tx, rx, ARRAY_SIZE(tx));
    g_assert_cmphex(rx[1], ==, 0xef);
    g_assert_cmphex(rx[2], ==, 0x40);
    g_assert_cmphex(rx[3], ==, 0x19);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_flash_read_3byte_address(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t data[8];
    static const uint8_t expected[8] = {
        0xa5, 0x5a, 0x3c, 0xc3, 0x11, 0x22, 0x33, 0x44,
    };

    flash_read(qts, FLASH_CMD_READ, K230_SSI_FLASH_PATTERN_ADDR,
               3, 0, data, sizeof(data));
    g_assert_cmpmem(data, sizeof(data), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_flash_read_4byte_address(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t data[4];
    static const uint8_t expected[4] = { 0x71, 0x72, 0x73, 0x74 };

    flash_read(qts, FLASH_CMD_READ4, K230_SSI_FLASH_HIGH_ADDR,
               4, 0, data, sizeof(data));
    g_assert_cmpmem(data, sizeof(data), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_page_program_and_readback(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t tx[4 + sizeof(payload)];
    uint8_t actual[sizeof(payload)];
    uint32_t addr = K230_SSI_FLASH_PROGRAM_ADDR;

    flash_write_enable(qts);
    tx[0] = FLASH_CMD_PP;
    tx[1] = addr >> 16;
    tx[2] = addr >> 8;
    tx[3] = addr;
    memcpy(tx + 4, payload, sizeof(payload));
    flash_transaction(qts, tx, NULL, sizeof(tx));
    flash_wait_ready(qts);

    flash_read(qts, FLASH_CMD_READ, addr, 3, 0, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), payload, sizeof(payload));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_sector_erase_and_readback(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t addr = K230_SSI_FLASH_ERASE_ADDR;
    uint8_t tx[] = {
        FLASH_CMD_SE, addr >> 16, addr >> 8, addr,
    };
    uint8_t actual[16];
    uint8_t expected[16];

    memset(expected, 0xff, sizeof(expected));
    flash_write_enable(qts);
    flash_transaction(qts, tx, NULL, sizeof(tx));
    flash_wait_ready(qts);

    flash_read(qts, FLASH_CMD_READ, addr, 3, 0, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_chip_select_restarts_command(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t tx[] = { FLASH_CMD_JEDEC, 0 };
    uint8_t first[ARRAY_SIZE(tx)];
    uint8_t second[ARRAY_SIZE(tx)];

    flash_transaction(qts, tx, first, ARRAY_SIZE(tx));
    flash_transaction(qts, tx, second, ARRAY_SIZE(tx));
    g_assert_cmphex(first[1], ==, 0xef);
    g_assert_cmphex(second[1], ==, 0xef);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void configure_enhanced_read(QTestState *qts, uint32_t frf,
                                    uint32_t trans_type, uint32_t wait_cycles,
                                    uint8_t opcode, uint32_t address,
                                    size_t read_len)
{
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_RO,
                       8, read_len - 1);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= frf << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);

    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(trans_type) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_WAIT(wait_cycles);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);

    /* SDK 非 IDMA 路径先使能并写指令/地址，再拉起 SER。 */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, opcode);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, address);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 1);
}

static void test_quad_output_read_sequence(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 0, 8,
                            FLASH_CMD_QUAD_OUT,
                            K230_SSI_FLASH_PATTERN_ADDR,
                            ARRAY_SIZE(actual));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, ARRAY_SIZE(actual));
    for (int i = 0; i < ARRAY_SIZE(actual); i++) {
        actual[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_io_mode_bits_and_dummy(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t spi_ctrlr0;

    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_MODE_BITS, 0xa5);
    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 1, 8,
                            FLASH_CMD_QUAD_IO,
                            K230_SSI_FLASH_PATTERN_ADDR, 4);
    k230_ssi_disable(qts, K230_SPI0_BASE);
    spi_ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE,
                                K230_SSI_SPI_CTRLR0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0,
                    spi_ctrlr0 | K230_SSI_SPI_CTRLR0_XIP_MD_EN);

    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE,
                                  K230_SSI_SPI_CTRLR0) &
                    (K230_SSI_SPI_CTRLR0_XIP_MD_EN |
                     K230_SSI_SPI_CTRLR0_WAIT(0x1f)),
                    ==, K230_SSI_SPI_CTRLR0_XIP_MD_EN |
                        K230_SSI_SPI_CTRLR0_WAIT(8));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_unsupported_octal_ddr_rxds_do_not_transfer(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_RO, 8, 3);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0) |
             (K230_SSI_FRF_OCTAL << K230_SSI_CTRLR0_SPI_FRF_SHIFT);
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_SPI_DDR_EN |
                 K230_SSI_SPI_CTRLR0_INST_DDR_EN |
                 K230_SSI_SPI_CTRLR0_RXDS_EN;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_enable_cs(qts, K230_SPI0_BASE, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_JEDEC);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

void k230_ssi_register_flash_tests(void)
{
    qtest_add_func("/k230-dw-ssi/flash/jedec-id", test_jedec_id);
    qtest_add_func("/k230-dw-ssi/flash/read-3byte",
                   test_flash_read_3byte_address);
    qtest_add_func("/k230-dw-ssi/flash/read-4byte",
                   test_flash_read_4byte_address);
    qtest_add_func("/k230-dw-ssi/flash/page-program",
                   test_page_program_and_readback);
    qtest_add_func("/k230-dw-ssi/flash/sector-erase",
                   test_sector_erase_and_readback);
    qtest_add_func("/k230-dw-ssi/flash/cs-restarts-command",
                   test_chip_select_restarts_command);
    qtest_add_func("/k230-dw-ssi/qspi/quad-output-read",
                   test_quad_output_read_sequence);
    qtest_add_func("/k230-dw-ssi/qspi/mode-bits-dummy",
                   test_quad_io_mode_bits_and_dummy);
    qtest_add_func("/k230-dw-ssi/qspi/unsupported-octal-ddr-rxds",
                   test_unsupported_octal_ddr_rxds_do_not_transfer);
}
