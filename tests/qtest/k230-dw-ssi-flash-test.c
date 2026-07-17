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

#define K230_SSI_SPI_CTRLR0_INST_L_16    (3U << 8)

static void flash_write_transaction(QTestState *qts,
                                    const uint8_t *command,
                                    size_t command_len)
{
    /*
     * 这是 Flash 的统一 Data-OUT 入口：command 中按 Flash 线协议排列
     * opcode、地址和待写入数据，函数只负责通过 SSI 发出这些字节。
     * 因为 TX_ONLY 不产生需要消费的 RX 数据，所以不会把“发送阶段的
     * 回读值”混入测试结果。
     *
     * 必须保持的顺序：
     *   configure disabled -> SER=0、SSIENR=1 -> 预填 command[]
     *   -> SER=BIT(0) -> 等 TXFLR=0/BUSY=0 -> SSIENR=0。
     *
     * SSIENR=0 产生 CS 上升沿；WREN、Page Program 和 Sector Erase
     * 都依赖这个事务结束边界。不要用旧的 TR helper 代替 TO。
     */
    g_assert_nonnull(command);
    g_assert_cmpuint(command_len, >, 0);
    g_assert_cmpuint(command_len, <=, K230_SSI_FIFO_DEPTH);

    /* TO 只发送 TX FIFO 中的帧，不把线路返回值写入 RX FIFO。 */
    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);

    /*
     * 先撤销旧的 SER，再使能控制器。这样即使上一条事务结束时 SER
     * 仍为 1，也不会在命令预填前重新选中 Flash。
     */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);

    for (size_t i = 0; i < command_len; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, command[i]);
    }

    /* SER 的上升沿/下降沿由控制器模型转换为 Flash 的 CS 变化。 */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_TXFLR,
                       UINT32_MAX, 0);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);

    /* 禁用控制器，释放 CS，并清理本次事务残留的 FIFO/阶段状态。 */
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void flash_read_transaction(QTestState *qts,
                                   const uint8_t *command,
                                   size_t command_len,
                                   uint8_t *data, size_t data_len)
{
    /*
     * 这是 Flash 的统一 Data-IN 入口：先发送 command 中的 opcode、地址和
     * dummy 字节，再从 RX FIFO 取出 data_len 个有效数据字节。调用者不需要
     * 处理 SSI 的 CTRLR0/CTRLR1、SER 或 FIFO 时序，只描述一条 Flash 事务。
     *
     * command[] 只包含 opcode/address/协议 dummy；CTRLR1 写
     * data_len - 1。SER 拉起后，Patch 3 自动生成 data_len 个接收帧。
     * RX FIFO 中不应出现 command 阶段的返回值。
     *
     * data_len 必须大于 0；完成后等待 BUSY=0，再写 SSIENR=0 撤销 CS。
     */
    g_assert_nonnull(command);
    g_assert_nonnull(data);
    g_assert_cmpuint(command_len, >, 0);
    g_assert_cmpuint(command_len, <=, K230_SSI_FIFO_DEPTH);
    g_assert_cmpuint(data_len, >, 0);
    /* 当前 helper 在 RX FIFO 中一次收齐数据，因此不能让 FIFO 填满。 */
    g_assert_cmpuint(data_len, <, K230_SSI_FIFO_DEPTH);

    /*
     * NDF 编码的是“接收帧数减一”，所以 data_len=1 时必须写 0，
     * data_len=6 时必须写 5。
     */
    k230_ssi_configure(qts, K230_SPI0_BASE,
                       K230_SSI_TMOD_EEPROM_READ, 8, data_len - 1);

    /* 和 TX_ONLY 一样，先确保旧事务的 SER 不会影响命令预填。 */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);

    /* 命令阶段只写 opcode/address/dummy，不为数据阶段写 dummy 帧。 */
    for (size_t i = 0; i < command_len; i++) {
        k230_ssi_write_frame(qts, K230_SPI0_BASE, command[i]);
    }

    /*
     * SER 选中 Flash 后，Patch 3 会先消费 command[]，再根据 NDF 自动
     * 发送 data_len 个 dummy 帧，并把返回值放入 RX FIFO。
     */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_RXFLR,
                       UINT32_MAX, data_len);
    k230_ssi_wait_mask(qts, K230_SPI0_BASE, K230_SSI_SR,
                       K230_SSI_SR_BUSY, 0);

    for (size_t i = 0; i < data_len; i++) {
        data[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }

    /* 读完 RX FIFO 后再禁用，完成 CS 事务边界。 */
    k230_ssi_disable(qts, K230_SPI0_BASE);
}

static void flash_read(QTestState *qts, uint8_t opcode, uint32_t address,
                       unsigned int addr_bytes, unsigned int dummy_bytes,
                       uint8_t *data, size_t len)
{
    /*
     * 组装普通 SPI 读命令的前缀：opcode + 大端地址 + dummy 字节。
     * 地址宽度和 dummy 数量由测试用例传入，因此同一个 helper 可覆盖
     * 3-byte read、4-byte read 和带 dummy 的 fast read。
     */
    size_t prefix = 1 + addr_bytes + dummy_bytes;
    g_autofree uint8_t *command = g_new0(uint8_t, prefix);

    command[0] = opcode;
    for (unsigned int i = 0; i < addr_bytes; i++) {
        command[1 + i] = address >> (8 * (addr_bytes - i - 1));
    }
    memset(command + 1 + addr_bytes, 0xff, dummy_bytes);
    flash_read_transaction(qts, command, prefix, data, len);
}

static uint8_t flash_read_status(QTestState *qts)
{
    /* RDSR 是一字节命令、后一字节返回状态的最小 Data-IN 事务。 */
    uint8_t command = FLASH_CMD_RDSR;
    uint8_t status;

    flash_read_transaction(qts, &command, 1, &status, 1);
    return status;
}

static void flash_wait_ready(QTestState *qts)
{
    /*
     * 编程/擦除期间 Flash 将 WIP 置 1。通过重复读取状态寄存器等待设备
     * 完成，并推进虚拟时钟让 QEMU 的异步 Flash 操作有机会结束。
     */
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
    /*
     * WREN 必须是一条独立事务；事务结束时 CS 拉高，Flash 才会锁存 WEL。
     * 后续 Page Program 或 Sector Erase 还必须重新发出自己的事务。
     */
    uint8_t cmd = FLASH_CMD_WREN;

    flash_write_transaction(qts, &cmd, 1);
}

static void test_jedec_id(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint8_t command = FLASH_CMD_JEDEC;
    uint8_t id[6];

    flash_read_transaction(qts, &command, 1, id, sizeof(id));
    g_assert_cmphex(id[0], ==, 0xef);
    g_assert_cmphex(id[1], ==, 0x40);
    g_assert_cmphex(id[2], ==, 0x19);

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
    flash_write_transaction(qts, tx, sizeof(tx));
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
    flash_write_transaction(qts, tx, sizeof(tx));
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
    uint8_t command = FLASH_CMD_JEDEC;
    uint8_t first;
    uint8_t second;

    flash_read_transaction(qts, &command, 1, &first, 1);
    flash_read_transaction(qts, &command, 1, &second, 1);
    g_assert_cmphex(first, ==, 0xef);
    g_assert_cmphex(second, ==, 0xef);

    qtest_quit(qts);
    k230_ssi_flash_image_clear(&image);
}

static void test_failed_enhanced_reset_falls_back_to_read_id(void)
{
    K230SsiFlashImage image;
    QTestState *qts = k230_ssi_start_with_flash(&image);
    uint32_t ctrlr0;
    uint32_t spi_ctrlr0;
    uint32_t fifo;
    uint8_t id[6];

    /*
     * U-Boot spi_hw_init() 在 SSIENR=1 时通过 TXFTLR 回读探测 FIFO 深度。
     * TXFTLR/RXFTLR 是运行期水位阈值，不能和 CTRLR0 一样被配置锁阻止。
     */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    for (fifo = 1; fifo < K230_SSI_FIFO_DEPTH; fifo++) {
        k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_TXFTLR, fifo);
        if (k230_ssi_readl(qts, K230_SPI0_BASE,
                           K230_SSI_TXFTLR) != fifo) {
            break;
        }
    }
    g_assert_cmpuint(fifo, ==, K230_SSI_FIFO_DEPTH);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_TXFTLR, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_RXFTLR, fifo - 1);

    /*
     * 模拟控制器已经接收到 U-Boot 8D-8D-8D reset-enable 的保守场景：
     * 16-bit 0x6666 指令、Octal、DTR、TX_ONLY、无地址和数据。
     * Patch 4 不支持增强格式，因此 TX FIFO 项应保留且不产生线路传输。
     * 当前预构建 U-Boot 会更早在 supports_op() 返回 -ENOTSUPP；这里仍
     * 主动覆盖“已有未发送 TX 项”的更强清理边界。
     */
    k230_ssi_configure(qts, K230_SPI0_BASE, K230_SSI_TMOD_TO, 8, 0);
    ctrlr0 = k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_CTRLR0);
    ctrlr0 &= ~K230_SSI_CTRLR0_SPI_FRF_MASK;
    ctrlr0 |= K230_SSI_FRF_OCTAL << K230_SSI_CTRLR0_SPI_FRF_SHIFT;
    spi_ctrlr0 = K230_SSI_SPI_CTRLR0_TRANS_TYPE(2) |
                 K230_SSI_SPI_CTRLR0_INST_L_16 |
                 K230_SSI_SPI_CTRLR0_SPI_DDR_EN |
                 K230_SSI_SPI_CTRLR0_INST_DDR_EN;
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SPI_CTRLR0, spi_ctrlr0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, 0x6666);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 1);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);

    /* U-Boot 失败退出先撤销 CS；下一条 exec_op 开头禁用 SSI。 */
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_disable(qts, K230_SPI0_BASE);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_TXFLR),
                     ==, 0);
    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, 0);

    /* 回退到 Standard EEPROM_READ，Read-ID 必须获得 NDF+1=6 字节。 */
    k230_ssi_configure(qts, K230_SPI0_BASE,
                       K230_SSI_TMOD_EEPROM_READ, 8, ARRAY_SIZE(id) - 1);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, 0);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SSIENR, 1);
    k230_ssi_write_frame(qts, K230_SPI0_BASE, FLASH_CMD_JEDEC);
    k230_ssi_writel(qts, K230_SPI0_BASE, K230_SSI_SER, BIT(0));

    g_assert_cmpuint(k230_ssi_readl(qts, K230_SPI0_BASE, K230_SSI_RXFLR),
                     ==, ARRAY_SIZE(id));
    for (size_t i = 0; i < ARRAY_SIZE(id); i++) {
        id[i] = k230_ssi_read_frame(qts, K230_SPI0_BASE);
    }
    g_assert_cmphex(id[0], ==, 0xef);
    g_assert_cmphex(id[1], ==, 0x40);
    g_assert_cmphex(id[2], ==, 0x19);

    k230_ssi_disable(qts, K230_SPI0_BASE);
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
    qtest_add_func("/k230-dw-ssi/flash/enhanced-reset-fallback-read-id",
                   test_failed_enhanced_reset_falls_back_to_read_id);
    qtest_add_func("/k230-dw-ssi/qspi/quad-output-read",
                   test_quad_output_read_sequence);
    qtest_add_func("/k230-dw-ssi/qspi/mode-bits-dummy",
                   test_quad_io_mode_bits_and_dummy);
    qtest_add_func("/k230-dw-ssi/qspi/unsupported-octal-ddr-rxds",
                   test_unsupported_octal_ddr_rxds_do_not_transfer);
}
