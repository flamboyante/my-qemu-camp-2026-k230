/*
 * QTest for the K230 Reset Management Unit.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "libqtest.h"
#include "hw/misc/k230_rmu.h"

#define K230_RMU_BASE 0x91101000ULL

static inline uint32_t rd(QTestState *qts, uint64_t off)
{
    return qtest_readl(qts, K230_RMU_BASE + off);
}
static inline void wr(QTestState *qts, uint64_t off, uint32_t val)
{
    qtest_writel(qts, K230_RMU_BASE + off, val);
}

/*
 * Documented reset values (K230 TRM chapter 2.1.5, per-bit "Reset" column).
 * The *_rst_done status bits reset to 0, so these differ from each register's
 * summarised "Total Reset Value".
 */
static void test_reset_values(void)
{
    QTestState *qts = qtest_init("-machine k230");

    /* Control registers. */
    g_assert_cmphex(rd(qts, K230_RMU_CPU0_CTRL),  ==, 0x00000000);
    g_assert_cmphex(rd(qts, K230_RMU_CPU1_CTRL),  ==, 0x00000001);
    g_assert_cmphex(rd(qts, K230_RMU_AI_CTRL),    ==, 0x00000000);
    g_assert_cmphex(rd(qts, K230_RMU_PERI0_CTRL), ==, 0x000ff0ff);
    g_assert_cmphex(rd(qts, K230_RMU_PERI1_CTRL), ==, 0x007e7fff);
    g_assert_cmphex(rd(qts, K230_RMU_HISYS_CTRL), ==, 0x00000000);
    g_assert_cmphex(rd(qts, K230_RMU_SRAM_CTRL),  ==, 0x00000002);
    g_assert_cmphex(rd(qts, K230_RMU_ISP_CTRL),   ==, 0x0000039f);

    /* A representative reset-time-control register. */
    g_assert_cmphex(rd(qts, K230_RMU_CPU0_TIM),   ==, 0x00fff880);
    g_assert_cmphex(rd(qts, K230_RMU_USB_TIM),    ==, 0x0008d288);

    qtest_quit(qts);
}

/*
 * Reset-time-control registers are plain read/write storage: they come up with
 * their documented value and accept arbitrary writes with no side effects.
 */
static void test_tim_storage(void)
{
    QTestState *qts = qtest_init("-machine k230");

    g_assert_cmphex(rd(qts, K230_RMU_MCTL_TIM), ==, 0x00000304);
    wr(qts, K230_RMU_MCTL_TIM, 0x12345678);
    g_assert_cmphex(rd(qts, K230_RMU_MCTL_TIM), ==, 0x12345678);

    qtest_quit(qts);
}

/*
 * Reserved bits are read-only zero: a write to an undocumented bit position is
 * dropped. ISP_CTL documents bits 0-9 and 28-29; bit 15 is reserved.
 */
static void test_reserved_bits(void)
{
    QTestState *qts = qtest_init("-machine k230");
    uint32_t before = rd(qts, K230_RMU_ISP_CTRL);

    wr(qts, K230_RMU_ISP_CTRL, before | BIT(15));
    g_assert_cmphex(rd(qts, K230_RMU_ISP_CTRL) & BIT(15), ==, 0);

    qtest_quit(qts);
}

/*
 * CPU0: a low-half write without its strobe is ignored; with the strobe the
 * reset request fires, latches done (bit12), and the request bit auto-clears.
 */
static void test_cpu_write_enable(void)
{
    QTestState *qts = qtest_init("-machine k230");

    /* No strobe: the low-half write is dropped, reads back as 0. */
    wr(qts, K230_RMU_CPU0_CTRL, K230_RMU_CPU_RESET);
    g_assert_cmphex(rd(qts, K230_RMU_CPU0_CTRL), ==, 0);

    /* With strobe (bit16): request bit0 takes effect, done (bit12) latches. */
    wr(qts, K230_RMU_CPU0_CTRL,
       K230_RMU_CPU_RESET | (K230_RMU_CPU_RESET << K230_RMU_WE_SHIFT));
    g_assert_cmphex(rd(qts, K230_RMU_CPU0_CTRL), ==, K230_RMU_CPU_DONE);

    qtest_quit(qts);
}

/*
 * HW_DONE: no write-enable needed. Writing HISYS request bit0 latches its
 * done bit (bit4); writing 1 to the done bit clears it.
 */
static void test_hw_done_and_w1c(void)
{
    QTestState *qts = qtest_init("-machine k230");
    uint32_t v;

    wr(qts, K230_RMU_HISYS_CTRL, BIT(0));      /* no strobe */
    v = rd(qts, K230_RMU_HISYS_CTRL);
    g_assert_cmphex(v & BIT(4), ==, BIT(4));   /* done latched */
    g_assert_cmphex(v & BIT(0), ==, 0);        /* request bit auto-cleared */

    wr(qts, K230_RMU_HISYS_CTRL, BIT(4));      /* write-1-to-clear */
    g_assert_cmphex(rd(qts, K230_RMU_HISYS_CTRL) & BIT(4), ==, 0);

    qtest_quit(qts);
}

/* Repeated resets: one HW_DONE line can be triggered again and again. */
static void test_repeat_reset(void)
{
    QTestState *qts = qtest_init("-machine k230");

    for (int i = 0; i < 3; i++) {
        wr(qts, K230_RMU_AI_CTRL, BIT(0));                 /* request */
        g_assert_cmphex(rd(qts, K230_RMU_AI_CTRL) & BIT(31), ==, BIT(31));
        wr(qts, K230_RMU_AI_CTRL, BIT(31));                /* W1C done */
        g_assert_cmphex(rd(qts, K230_RMU_AI_CTRL) & BIT(31), ==, 0);
    }

    qtest_quit(qts);
}

/* FLUSH: the CPU0 bit4 flush request is auto-cleared, reads back as 0. */
static void test_flush_auto_clear(void)
{
    QTestState *qts = qtest_init("-machine k230");

    wr(qts, K230_RMU_CPU0_CTRL,
       K230_RMU_CPU_FLUSH | (K230_RMU_CPU_FLUSH << K230_RMU_WE_SHIFT));
    g_assert_cmphex(rd(qts, K230_RMU_CPU0_CTRL) & K230_RMU_CPU_FLUSH, ==, 0);

    qtest_quit(qts);
}

/* SW_DONE: the PERI0 low half is plain storage, no strobe and no done bit. */
static void test_sw_done_storage(void)
{
    QTestState *qts = qtest_init("-machine k230");

    /* WDT0 / WDT1 reset bits */
    wr(qts, K230_RMU_PERI0_CTRL, BIT(12) | BIT(13));
    g_assert_cmphex(rd(qts, K230_RMU_PERI0_CTRL), ==, (BIT(12) | BIT(13)));

    qtest_quit(qts);
}

/*
 * CPU1 needs separate assert and deassert operations: unlike CPU0 its reset
 * request bit does NOT self-clear. Assert (bit0 + strobe) latches done (bit12)
 * and keeps bit0 set; deassert (strobe only, bit0 low) clears bit0.
 */
static void test_cpu1_two_step(void)
{
    QTestState *qts = qtest_init("-machine k230");

    /* Assert: request bit0 stays set AND done bit12 latches. */
    wr(qts, K230_RMU_CPU1_CTRL,
       K230_RMU_CPU_RESET | (K230_RMU_CPU_RESET << K230_RMU_WE_SHIFT));
    g_assert_cmphex(rd(qts, K230_RMU_CPU1_CTRL), ==,
                    K230_RMU_CPU_RESET | K230_RMU_CPU_DONE);

    /* Deassert: drive bit0 low with its strobe; bit0 clears, done stays. */
    wr(qts, K230_RMU_CPU1_CTRL, K230_RMU_CPU_RESET << K230_RMU_WE_SHIFT);
    g_assert_cmphex(rd(qts, K230_RMU_CPU1_CTRL), ==, K230_RMU_CPU_DONE);

    /* Done is write-1-to-clear (gated by its strobe). */
    wr(qts, K230_RMU_CPU1_CTRL,
       K230_RMU_CPU_DONE | (K230_RMU_CPU_DONE << K230_RMU_WE_SHIFT));
    g_assert_cmphex(rd(qts, K230_RMU_CPU1_CTRL), ==, 0);

    qtest_quit(qts);
}

/*
 * Reset propagation: asserting a watchdog reset line in PERI0_CTRL performs a
 * real cold reset of the linked WDT device. Put WDT0 into a non-default state
 * (TORR != 0), pulse the RMU, and confirm WDT0 came back to its reset value.
 */
#define K230_WDT0_BASE 0x91106000ULL
#define K230_WDT_TORR  0x04

static void test_wdt_reset_propagation(void)
{
    QTestState *qts = qtest_init("-machine k230");

    /* Dirty WDT0: TORR reset value is 0, write a non-zero timeout range. */
    qtest_writel(qts, K230_WDT0_BASE + K230_WDT_TORR, 0xf);
    g_assert_cmphex(qtest_readl(qts, K230_WDT0_BASE + K230_WDT_TORR), ==, 0xf);

    /* Assert WDT0's software reset line through the RMU (PERI0 bit12). */
    wr(qts, K230_RMU_PERI0_CTRL, K230_RMU_PERI0_WDT0_RST);

    /* WDT0 must be back to its reset value. */
    g_assert_cmphex(qtest_readl(qts, K230_WDT0_BASE + K230_WDT_TORR), ==, 0);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/k230/rmu/reset_values",   test_reset_values);
    qtest_add_func("/k230/rmu/tim_storage",    test_tim_storage);
    qtest_add_func("/k230/rmu/reserved_bits",  test_reserved_bits);
    qtest_add_func("/k230/rmu/cpu_we",         test_cpu_write_enable);
    qtest_add_func("/k230/rmu/cpu1_two_step",  test_cpu1_two_step);
    qtest_add_func("/k230/rmu/hw_done_w1c",    test_hw_done_and_w1c);
    qtest_add_func("/k230/rmu/repeat_reset",   test_repeat_reset);
    qtest_add_func("/k230/rmu/flush",          test_flush_auto_clear);
    qtest_add_func("/k230/rmu/sw_done",        test_sw_done_storage);
    qtest_add_func("/k230/rmu/wdt_reset_prop", test_wdt_reset_propagation);

    return g_test_run();
}
