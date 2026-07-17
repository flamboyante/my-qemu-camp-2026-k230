/*
 * K230 DWC SSI Dual/Quad QSPI qtest
 *
 * 测试通过真实 w25q256 验证增强事务生成结果。QEMU SSIBus 是
 * 字节级事务接口。
 * 本文件验证阶段顺序与数据，不模拟 IO 线电平。
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

#define FLASH_CMD_WREN          0x06
#define FLASH_CMD_RDSR          0x05
#define FLASH_CMD_DUAL_OUT      0x3b
#define FLASH_CMD_QUAD_OUT      0x6b
#define FLASH_CMD_QUAD_IO       0xeb
#define FLASH_CMD_QUAD_PP       0x32
#define FLASH_CMD_JEDEC         0x9f
#define FLASH_SR_WIP            BIT(0)

static void configure_enhanced_transfer(QTestState *qts, uint32_t tmod,
                                        uint32_t frf, uint32_t trans_type,
                                        uint32_t wait_cycles,
                                        bool mode_bits_enabled,
                                        size_t data_frames)
{
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    g_assert_cmpuint(data_frames, >, 0);
    g_assert_cmpuint(data_frames, <=, UINT16_MAX + 1ULL);

    k230_ssi_configure(qts, K230_SPI0_BASE, tmod, 8, data_frames - 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);

    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= frf << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);

    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(trans_type) |
                 K230_SSI_SPI_CTRLR0_ADDR_L(24) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 K230_SSI_SPI_CTRLR0_WAIT(wait_cycles);
    if (mode_bits_enabled) {
        spi_ctrlr0 |= K230_SSI_SPI_CTRLR0_XIP_MD_EN |
                      K230_SSI_SPI_CTRLR0_XIP_MBL_8;
    }
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
}

static void start_enhanced_transfer(QTestState *qts,
                                    uint8_t opcode, uint32_t address)
{
    /* SDK PIO 顺序：先使能并预填控制字段，再选择 CS。 */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, opcode);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, address);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
}

static void configure_enhanced_read(QTestState *qts, uint32_t frf,
                                    uint32_t trans_type, uint32_t wait_cycles,
                                    bool mode_bits_enabled,
                                    uint8_t opcode, uint32_t address,
                                    size_t read_len)
{
    configure_enhanced_transfer(qts, K230_SSI_TMOD_RO, frf, trans_type,
                                wait_cycles, mode_bits_enabled, read_len);
    start_enhanced_transfer(qts, opcode, address);
}

static void standard_write_transaction(QTestState *qts,
                                       const uint8_t *frames, size_t len)
{
    g_assert_nonnull(frames);
    g_assert_cmpuint(len, >, 0);
    g_assert_cmpuint(len, <=, K230_SSI_FIFO_DEPTH);

    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);

    for (size_t i = 0; i < len; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, frames[i]);
    }
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_TXFLR,
                       UINT32_MAX, 0);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static uint8_t standard_read_status(QTestState *qts)
{
    uint8_t status;

    k230_ssi_configure(qts, K230_SPI0_BASE,
                       K230_SSI_TMOD_EEPROM_READ, 8, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_RDSR);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, 1);
    status = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    k230_ssi_disable(qts, K230_SPI0_BASE);
    return status;
}

static void standard_wait_ready(QTestState *qts)
{
    for (int i = 0; i < 1000; i++) {
        if (!(standard_read_status(qts) & FLASH_SR_WIP)) {
            return;
        }
        qtest_clock_step(qts, 1000000);
    }

    g_assert_cmphex(standard_read_status(qts) & FLASH_SR_WIP, ==, 0);
}

static void read_enhanced_result(QTestState *qts, uint8_t *data, size_t len)
{
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, len);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);

    for (size_t i = 0; i < len; i++) {
        data[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void assert_enhanced_read(QTestState *qts, uint32_t frf,
                                 uint8_t opcode, uint32_t trans_type)
{
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    configure_enhanced_read(qts, frf, trans_type, 8, false,
                            opcode, K230_SSI_FLASH_PATTERN_ADDR,
                            ARRAY_SIZE(actual));
    read_enhanced_result(qts, actual, ARRAY_SIZE(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));
}

static void test_dual_and_quad_output_read(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    /* 0x3b 是 1-1-2，0x6b 是 1-1-4；两者共用 TT0。 */
    assert_enhanced_read(qts, K230_SSI_FRF_DUAL,
                         FLASH_CMD_DUAL_OUT, 0);
    assert_enhanced_read(qts, K230_SSI_FRF_QUAD,
                         FLASH_CMD_QUAD_OUT, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_page_program_streaming(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t actual[ARRAY_SIZE(payload)];
    uint8_t wren = FLASH_CMD_WREN;
    uint32_t addr = K230_SSI_FLASH_PROGRAM_ADDR;

    standard_write_transaction(qts, &wren, 1);
    configure_enhanced_transfer(qts, K230_SSI_TMOD_TO,
                                K230_SSI_FRF_QUAD, 0, 0, false,
                                ARRAY_SIZE(payload));
    start_enhanced_transfer(qts, FLASH_CMD_QUAD_PP, addr);

    /* 前缀已发送但 payload 尚未到达。 */
    /* TX FIFO 空不等于事务完成。 */
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_TXFLR), ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    for (size_t i = 0; i < ARRAY_SIZE(payload) / 2; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, payload[i]);
    }
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, K230_SSI_SR_BUSY);

    for (size_t i = ARRAY_SIZE(payload) / 2;
         i < ARRAY_SIZE(payload); i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, payload[i]);
    }
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_TXFLR,
                       UINT32_MAX, 0);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_RXFLR), ==, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);

    standard_wait_ready(qts);
    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 0, 8, false,
                            FLASH_CMD_QUAD_OUT, addr,
                            ARRAY_SIZE(actual));
    read_enhanced_result(qts, actual, ARRAY_SIZE(actual));
    g_assert_cmpmem(actual, sizeof(actual), payload, sizeof(payload));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_enhanced_rx_fifo_resume(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t status;

    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 0, 8, false,
                            FLASH_CMD_QUAD_OUT,
                            K230_SSI_FLASH_PATTERN_ADDR,
                            K230_SSI_FIFO_DEPTH + 1);

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_RXFLR), ==,
                     K230_SSI_FIFO_DEPTH);
    status = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR);
    g_assert_cmphex(status & (K230_SSI_SR_BUSY | K230_SSI_SR_RFF), ==,
                    K230_SSI_SR_BUSY | K230_SSI_SR_RFF);

    /*
     * 第一次 pop 腾出空间并恢复最后一帧。
     * RXFLR 保持满，BUSY 清零。
     */
    g_assert_cmphex(k230_ssi_read_frame(qts, K230_SPI0_BASE), ==, 0xa5);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_RXFLR), ==,
                     K230_SSI_FIFO_DEPTH);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    for (int i = 0; i < K230_SSI_FIFO_DEPTH; i++) {
        k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    k230_ssi_disable(qts, K230_SPI0_BASE);
    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_enhanced_prefix_is_atomic(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t data[4];

    configure_enhanced_transfer(qts, K230_SSI_TMOD_RO,
                                K230_SSI_FRF_QUAD, 0, 8, false,
                                ARRAY_SIZE(data));
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));

    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_QUAD_OUT);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE,
                                    K230_SSI_TXFLR), ==, 1);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    k230_ssi_write_frame(qts, K230_SPI0_BASE,
                         K230_SSI_FLASH_PATTERN_ADDR);
    read_enhanced_result(qts, data, ARRAY_SIZE(data));
    g_assert_cmphex(data[0], ==, 0xa5);
    g_assert_cmphex(data[1], ==, 0x5a);
    g_assert_cmphex(data[2], ==, 0x3c);
    g_assert_cmphex(data[3], ==, 0xc3);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_quad_io_mode_bits_and_dummy(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    static const uint8_t expected[] = { 0xa5, 0x5a, 0x3c, 0xc3 };
    uint8_t actual[ARRAY_SIZE(expected)];

    /*
     * Winbond 0xeb 是 1-4-4，并包含一个 mode byte 和四个 dummy
     * cycle。m25p80 将 dummy cycle 建模为字节写入，因此这项测试
     * 必须真正读数，不能只检查相关寄存器是否能读回。
     */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_XIP_MODE_BITS, 0xa5);
    configure_enhanced_read(qts, K230_SSI_FRF_QUAD, 1, 4, true,
                            FLASH_CMD_QUAD_IO,
                            K230_SSI_FLASH_PATTERN_ADDR,
                            ARRAY_SIZE(actual));
    read_enhanced_result(qts, actual, ARRAY_SIZE(actual));
    g_assert_cmpmem(actual, sizeof(actual), expected, sizeof(expected));

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void assert_enhanced_config_rejected(QTestState *qts,
                                            uint32_t tmod, uint32_t frf,
                                            uint32_t trans_type,
                                            uint32_t extra_spi_ctrlr0)
{
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;

    k230_ssi_configure(qts, K230_SPI0_BASE, tmod, 8, 0);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= frf << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(trans_type) |
                 K230_SSI_SPI_CTRLR0_INST_L_8 |
                 extra_spi_ctrlr0;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_enable_cs(qts, K230_SPI0_BASE, BIT(0));
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_JEDEC);

    /* 地址长度为 0；错误接受时 instruction 会被消费。 */
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 1);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);
    g_assert_cmphex(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_SR) &
                    K230_SSI_SR_BUSY, ==, 0);

    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void test_unsupported_enhanced_configs_do_not_transfer(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);

    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_RO,
                                    K230_SSI_FRF_OCTAL, 0, 0);
    assert_enhanced_config_rejected(
        qts, K230_SSI_TMOD_RO, K230_SSI_FRF_QUAD, 0,
        K230_SSI_SPI_CTRLR0_SPI_DDR_EN |
        K230_SSI_SPI_CTRLR0_INST_DDR_EN |
        K230_SSI_SPI_CTRLR0_RXDS_EN |
        K230_SSI_SPI_CTRLR0_RXDS_SIG_EN);
    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_RO,
                                    K230_SSI_FRF_QUAD, 3, 0);
    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_TR,
                                    K230_SSI_FRF_QUAD, 0, 0);
    assert_enhanced_config_rejected(qts, K230_SSI_TMOD_EEPROM_READ,
                                    K230_SSI_FRF_QUAD, 0, 0);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

void k230_ssi_register_qspi_tests(void)
{
    qtest_add_func("/k230-dw-ssi/qspi/dual-quad-output-read",
                   test_dual_and_quad_output_read);
    qtest_add_func("/k230-dw-ssi/qspi/mode-bits-dummy",
                   test_quad_io_mode_bits_and_dummy);
    qtest_add_func("/k230-dw-ssi/qspi/quad-page-program",
                   test_quad_page_program_streaming);
    qtest_add_func("/k230-dw-ssi/qspi/rx-fifo-resume",
                   test_enhanced_rx_fifo_resume);
    qtest_add_func("/k230-dw-ssi/qspi/prefix-atomic",
                   test_enhanced_prefix_is_atomic);
    qtest_add_func("/k230-dw-ssi/qspi/unsupported-configs",
                   test_unsupported_enhanced_configs_do_not_transfer);
}
