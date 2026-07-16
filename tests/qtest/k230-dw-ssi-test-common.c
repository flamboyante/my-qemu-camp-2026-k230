/*
 * K230 DWC SSI qtest 公共工具
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "k230-dw-ssi-test.h"

const K230SsiInstance k230_ssi_instances[3] = {
    {
        .name = "spi0",
        .base = K230_SPI0_BASE,
        .num_cs = 1,
        .max_lines = 8,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_FMC_RESET,
        .first_irq = 146,
        .has_xip = true,
    }, {
        .name = "spi1",
        .base = K230_SPI1_BASE,
        .num_cs = 5,
        .max_lines = 4,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_SPI_RESET,
        .first_irq = 155,
        .has_xip = false,
    }, {
        .name = "spi2",
        .base = K230_SPI2_BASE,
        .num_cs = 5,
        .max_lines = 4,
        .spi_ctrlr0_reset = K230_SSI_SPI_CTRLR0_SPI_RESET,
        .first_irq = 164,
        .has_xip = false,
    },
};

QTestState *k230_ssi_start(void)
{
    return qtest_init("-machine k230");
}

static void write_exact(int fd, const void *buf, size_t len, off_t offset)
{
    ssize_t ret = pwrite(fd, buf, len, offset);

    g_assert_cmpint(ret, ==, len);
}

void k230_ssi_flash_image_init(K230SsiFlashImage *image)
{
    static const uint8_t low_pattern[] = {
        0xa5, 0x5a, 0x3c, 0xc3, 0x11, 0x22, 0x33, 0x44,
    };
    static const uint8_t high_pattern[] = { 0x71, 0x72, 0x73, 0x74 };
    uint8_t erased[4096];
    uint8_t dirty[4096];
    int fd;

    memset(erased, 0xff, sizeof(erased));
    memset(dirty, 0x00, sizeof(dirty));

    image->path = NULL;
    fd = g_file_open_tmp("qtest.k230.w25q256.XXXXXX", &image->path, NULL);
    g_assert_cmpint(fd, >=, 0);
    g_assert_cmpint(ftruncate(fd, K230_SSI_FLASH_IMAGE_SIZE), ==, 0);

    write_exact(fd, low_pattern, sizeof(low_pattern),
                K230_SSI_FLASH_PATTERN_ADDR);
    write_exact(fd, high_pattern, sizeof(high_pattern),
                K230_SSI_FLASH_HIGH_ADDR);
    write_exact(fd, erased, sizeof(erased), K230_SSI_FLASH_PROGRAM_ADDR);
    write_exact(fd, dirty, sizeof(dirty), K230_SSI_FLASH_ERASE_ADDR);
    close(fd);
}

void k230_ssi_flash_image_clear(K230SsiFlashImage *image)
{
    if (image->path) {
        unlink(image->path);
        g_clear_pointer(&image->path, g_free);
    }
}

QTestState *k230_ssi_start_with_flash(K230SsiFlashImage *image)
{
    /*
     * 启动带真实 QEMU SPI NOR 的 K230 machine：spi-flash 选择设备模型，
     * if=mtd 的 drive 提供可持久化的原始 Flash 内容。测试结束后由调用者
     * 使用 k230_ssi_flash_image_clear() 删除临时镜像。
     */
    k230_ssi_flash_image_init(image);
    return qtest_initf("-machine k230,spi-flash=w25q256 "
                       "-drive file=%s,format=raw,if=mtd",
                       image->path);
}

uint32_t k230_ssi_readl(QTestState *qts, uint64_t base, uint32_t offset)
{
    return qtest_readl(qts, base + offset);
}

void k230_ssi_writel(QTestState *qts, uint64_t base,
                     uint32_t offset, uint32_t value)
{
    qtest_writel(qts, base + offset, value);
}

void k230_ssi_disable(QTestState *qts, uint64_t base)
{
    k230_ssi_writel(qts, base, K230_SSI_SSIENR, 0);
}

void k230_ssi_configure(QTestState *qts, uint64_t base,
                        uint32_t tmod, uint32_t dfs_bits, uint32_t ndf)
{
    uint32_t ctrlr0;

    g_assert_cmpuint(dfs_bits, >=, 4);
    g_assert_cmpuint(dfs_bits, <=, 32);
    g_assert_cmpuint(tmod, <=, K230_SSI_TMOD_EEPROM_READ);

    k230_ssi_disable(qts, base);
    ctrlr0 = (dfs_bits - 1) & K230_SSI_CTRLR0_DFS_MASK;
    ctrlr0 |= tmod << K230_SSI_CTRLR0_TMOD_SHIFT;
    k230_ssi_writel(qts, base, K230_SSI_CTRLR0, ctrlr0);
    k230_ssi_writel(qts, base, K230_SSI_CTRLR1, ndf);
    k230_ssi_writel(qts, base, K230_SSI_BAUDR, 2);
}

void k230_ssi_enable_cs(QTestState *qts, uint64_t base, uint32_t ser)
{
    k230_ssi_writel(qts, base, K230_SSI_SER, ser);
    k230_ssi_writel(qts, base, K230_SSI_SSIENR, 1);
}

void k230_ssi_write_frame(QTestState *qts, uint64_t base, uint32_t value)
{
    k230_ssi_writel(qts, base, K230_SSI_DR0, value);
}

uint32_t k230_ssi_read_frame(QTestState *qts, uint64_t base)
{
    return k230_ssi_readl(qts, base, K230_SSI_DR0);
}

void k230_ssi_wait_mask(QTestState *qts, uint64_t base, uint32_t offset,
                        uint32_t mask, uint32_t expected)
{
    for (int i = 0; i < 1000; i++) {
        uint32_t value = k230_ssi_readl(qts, base, offset);

        if ((value & mask) == expected) {
            return;
        }
        qtest_clock_step(qts, 1000);
    }

    g_assert_cmphex(k230_ssi_readl(qts, base, offset) & mask, ==, expected);
}

void k230_ssi_drain_rx(QTestState *qts, uint64_t base)
{
    while (k230_ssi_readl(qts, base, K230_SSI_RXFLR)) {
        (void)k230_ssi_read_frame(qts, base);
    }
}

bool k230_ssi_plic_pending(QTestState *qts, uint32_t irq)
{
    uint64_t addr = K230_PLIC_BASE + K230_PLIC_PENDING_BASE +
                    (irq / 32) * sizeof(uint32_t);

    return qtest_readl(qts, addr) & BIT(irq % 32);
}

uint32_t k230_ssi_frame_mask(uint32_t dfs_bits)
{
    g_assert_cmpuint(dfs_bits, >=, 4);
    g_assert_cmpuint(dfs_bits, <=, 32);
    return dfs_bits == 32 ? UINT32_MAX : MAKE_64BIT_MASK(0, dfs_bits);
}

void k230_ssi_standard_transaction(QTestState *qts, uint64_t base,
                                   const uint8_t *tx, uint8_t *rx, size_t len)
{
    k230_ssi_configure(qts, base, K230_SSI_TMOD_TR, 8, 0);
    k230_ssi_enable_cs(qts, base, 1);

    for (size_t i = 0; i < len; i++) {
        k230_ssi_write_frame(qts, base, tx[i]);
    }
    k230_ssi_wait_mask(qts, base, K230_SSI_RXFLR, UINT32_MAX, len);
    for (size_t i = 0; i < len; i++) {
        uint8_t value = k230_ssi_read_frame(qts, base);

        if (rx) {
            rx[i] = value;
        }
    }
    k230_ssi_disable(qts, base);
}
