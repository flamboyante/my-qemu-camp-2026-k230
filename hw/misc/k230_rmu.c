/*
 * K230 Reset Management Unit (RMU / SYSCTL_RST)
 *
 * The RMU is a bank of reset-control registers at 0x91101000. Each register
 * gathers the software-controllable reset lines of a group of peripherals.
 * Register semantics are modelled after the five reset types described in the
 * Linux mainline driver drivers/reset/reset-k230.c:
 *
 *   - CPU0      : high-half write-enable strobe, a done bit (bit12); the reset
 *                 request is self-clearing.
 *   - CPU1      : same layout as CPU0 but the reset request is NOT self-
 *                 clearing: software must assert it and later deassert it as
 *                 two separate operations.
 *   - FLUSH     : bit4 of the CPU registers; hardware auto-clears it, no done.
 *   - HW_DONE   : no write-enable; a done bit is latched when the reset fires
 *                 and cleared by software (write-1-to-clear).
 *   - SW_DONE   : plain read/write storage, no write-enable and no done bit.
 *
 * For most groups QEMU has no real peripheral to reset, so the model collapses
 * the hardware reset latency to zero: writing a reset request bit latches the
 * matching done bit in the same access and (for self-clearing lines) auto-
 * clears the request bit, which lets the kernel's readl_poll_timeout() loops
 * succeed on the first read. For the watchdogs, which ARE modelled, asserting
 * their PERI0 reset bits additionally performs a real device_cold_reset() on
 * the linked WDT devices (still zero latency, guest-visible ordering intact).
 *
 * Copyright (c) 2026 Jack Wang <163wangjack@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "migration/vmstate.h"
#include "hw/core/qdev.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/k230_rmu.h"
#include "trace.h"

/* Sentinel meaning "this reset request bit has no associated done bit". */
#define K230_RMU_NO_DONE  0xff

/* A single (reset request bit -> done bit) pairing within one register. */
typedef struct {
    uint8_t reset_bit;   /* reset request bit index, 0..15               */
    uint8_t done_bit;    /* matching done bit index, or K230_RMU_NO_DONE  */
    /*
     * true:  the reset request auto-clears once the reset fires.
     * false: it stays asserted until software deasserts it (CPU1).
     */
    bool    self_clear;
} K230RmuPair;

/* Description of one control register. */
typedef struct {
    hwaddr             offset;        /* word-aligned register offset */
    bool               has_we;        /* high 16 bits are write-enable */
    const K230RmuPair *pairs;         /* reset/done pairs, NULL for SW_DONE */
    unsigned           n_pairs;       /* number of pairings (0 for SW_DONE) */
    uint32_t           reset_val;     /* TRM reset value (per-bit column) */
    uint32_t           writable_mask; /* writable bits; reserved bits are 0 */
} K230RmuReg;

/*
 * Per-register (reset bit -> done bit) tables, copied verbatim from the bit
 * positions in the Linux driver's k230_resets[] table. The last field marks
 * whether the reset request auto-clears (see K230RmuPair::self_clear).
 */
static const K230RmuPair pairs_cpu0[]    = { {0, 12, true},
                                             {4, K230_RMU_NO_DONE, true} };
/* CPU1 reset bit0 is NOT self-clearing: assert and deassert are two steps. */
static const K230RmuPair pairs_cpu1[]    = { {0, 12, false},
                                             {4, K230_RMU_NO_DONE, true} };
static const K230RmuPair pairs_ai[]      = { {0, 31, true} };
static const K230RmuPair pairs_vpu[]     = { {0, 31, true} };
/* done in low bits */
static const K230RmuPair pairs_hisys[]   = { {0, 4, true}, {1, 5, true} };
static const K230RmuPair pairs_sdio[]    = { {0, 28, true}, {1, 29, true},
                                             {2, 30, true} };
static const K230RmuPair pairs_usb[]     = { {0, 28, true}, {1, 29, true},
                                             {0, 30, true}, {1, 31, true} };
static const K230RmuPair pairs_spi[]     = { {0, 28, true}, {1, 29, true},
                                             {2, 30, true} };
static const K230RmuPair pairs_sec[]     = { {0, 31, true} };
static const K230RmuPair pairs_dma[]     = { {0, 28, true}, {1, 29, true} };
static const K230RmuPair pairs_decomp[]  = { {0, 31, true} };
/* bit1 SW_DONE */
static const K230RmuPair pairs_sram[]    = { {0, 28, true}, {2, 30, true},
                                             {3, 31, true} };
static const K230RmuPair pairs_nonai2d[] = { {0, 31, true} };
static const K230RmuPair pairs_mctl[]    = { {0, 31, true} };
/* other low bits SW_DONE */
static const K230RmuPair pairs_isp[]     = { {6, 29, true}, {5, 28, true} };
static const K230RmuPair pairs_dpu[]     = { {0, 31, true} };
static const K230RmuPair pairs_disp[]    = { {0, 31, true} };
static const K230RmuPair pairs_gpu[]     = { {0, 31, true} };
static const K230RmuPair pairs_audio[]   = { {0, 31, true} };

/*
 * Reset values and reserved-bit masks are taken from the K230 TRM (chapter
 * 2.1.5).
 *
 * reset_val uses the per-bit "Reset" column of each register (the status
 * "*_rst_done" bits reset to 0, so it can differ from the register's summarised
 * "Total Reset Value"). writable_mask lists the bits the TRM documents as
 * accessible; undocumented (reserved) bits read back as 0 and ignore writes.
 *
 * K230_RMU_REG():  a control register with a reset/done pairing table.
 * K230_RMU_SW():   a plain read/write storage bank (no reset/done pairs).
 * K230_RMU_TIM():  a reset-time-control register - plain storage, fully
 *                  writable, backed by its documented reset value.
 */
#define K230_RMU_REG(off, we, p, rst, wmask) \
    { (off), (we), (p), ARRAY_SIZE(p), (rst), (wmask) }
#define K230_RMU_SW(off, rst, wmask) \
    { (off), false, NULL, 0, (rst), (wmask) }
#define K230_RMU_TIM(off, rst) \
    { (off), false, NULL, 0, (rst), 0xffffffffu }

static const K230RmuReg k230_rmu_regs[] = {
    /* Control registers (offset, write-enable, pairs, reset_val, writable). */
    K230_RMU_REG(K230_RMU_CPU0_CTRL, true, pairs_cpu0, 0x00000000, 0x0000701f),
    K230_RMU_REG(K230_RMU_CPU1_CTRL, true, pairs_cpu1, 0x00000001, 0x00003011),
    K230_RMU_REG(K230_RMU_AI_CTRL, false, pairs_ai, 0x00000000, 0x80000001),
    K230_RMU_REG(K230_RMU_VPU_CTRL, false, pairs_vpu, 0x00000000, 0x80000001),
    K230_RMU_SW(K230_RMU_PERI0_CTRL, 0x000ff0ff, 0x000ff0ff),
    K230_RMU_SW(K230_RMU_PERI1_CTRL, 0x007e7fff, 0x007e7fff),
    K230_RMU_REG(K230_RMU_HISYS_CTRL, false, pairs_hisys, 0x00000000, 0x33),
    K230_RMU_REG(K230_RMU_SDIO_CTRL, false, pairs_sdio, 0x00000000, 0x70000007),
    K230_RMU_REG(K230_RMU_USB_CTRL, false, pairs_usb, 0x00000000, 0xf0000003),
    K230_RMU_REG(K230_RMU_SPI_CTRL, false, pairs_spi, 0x00000000, 0x70000007),
    K230_RMU_REG(K230_RMU_SEC_CTRL, false, pairs_sec, 0x00000000, 0x80000001),
    K230_RMU_REG(K230_RMU_DMA_CTRL, false, pairs_dma, 0x00000000, 0x30000003),
    K230_RMU_REG(K230_RMU_DECOMP_CTRL, false, pairs_decomp, 0x0, 0x80000001),
    K230_RMU_REG(K230_RMU_SRAM_CTRL, false, pairs_sram, 0x00000002, 0xd000000f),
    K230_RMU_REG(K230_RMU_NONAI2D_CTRL, false, pairs_nonai2d, 0x0, 0x80000001),
    K230_RMU_REG(K230_RMU_MCTL_CTRL, false, pairs_mctl, 0x00000000, 0x80000001),
    K230_RMU_REG(K230_RMU_ISP_CTRL, false, pairs_isp, 0x0000039f, 0x300003ff),
    K230_RMU_REG(K230_RMU_DPU_CTRL, false, pairs_dpu, 0x00000000, 0x80000001),
    K230_RMU_REG(K230_RMU_DISP_CTRL, false, pairs_disp, 0x00000000, 0x80000001),
    K230_RMU_REG(K230_RMU_GPU_CTRL, false, pairs_gpu, 0x00000000, 0x80000001),
    K230_RMU_REG(K230_RMU_AUDIO_CTRL, false, pairs_audio, 0x0, 0x80000001),
    K230_RMU_SW(K230_RMU_SPI2AXI_CTRL, 0x00000000, 0xffffffff),

    /* Reset-time-control registers (offset, reset_val). */
    K230_RMU_TIM(K230_RMU_CPU0_TIM, 0x00fff880),
    K230_RMU_TIM(K230_RMU_CPU1_TIM, 0x00066660),
    K230_RMU_TIM(K230_RMU_AI_TIM, 0x00000880),
    K230_RMU_TIM(K230_RMU_VPU_TIM, 0x00000880),
    K230_RMU_TIM(K230_RMU_HISYS_TIM, 0x00080800),
    K230_RMU_TIM(K230_RMU_SDCTL_TIM, 0x00080800),
    K230_RMU_TIM(K230_RMU_USB_TIM, 0x0008d288),
    K230_RMU_TIM(K230_RMU_SPI_TIM, 0x00080c00),
    K230_RMU_TIM(K230_RMU_SEC_TIM, 0x00080800),
    K230_RMU_TIM(K230_RMU_DMAC_TIM, 0x00040400),
    K230_RMU_TIM(K230_RMU_DECOMP_TIM, 0x00040400),
    K230_RMU_TIM(K230_RMU_SRAM_TIM, 0x00020200),
    K230_RMU_TIM(K230_RMU_NONAI2D_TIM, 0x00020200),
    K230_RMU_TIM(K230_RMU_MCTL_TIM, 0x00000304),
    K230_RMU_TIM(K230_RMU_ISP_TIM, 0x00030202),
    K230_RMU_TIM(K230_RMU_ISP_DW_TIM, 0x00020202),
    K230_RMU_TIM(K230_RMU_DPU_TIM, 0x00020200),
    K230_RMU_TIM(K230_RMU_DISP_TIM, 0x00040404),
    K230_RMU_TIM(K230_RMU_V2P5D_TIM, 0x00040404),
    K230_RMU_TIM(K230_RMU_AUDIO_TIM, 0x00000880),
};

/* Look up the register description for a word-aligned offset, or NULL. */
static const K230RmuReg *k230_rmu_lookup(hwaddr offset)
{
    for (size_t i = 0; i < ARRAY_SIZE(k230_rmu_regs); i++) {
        if (k230_rmu_regs[i].offset == offset) {
            return &k230_rmu_regs[i];
        }
    }
    return NULL;
}

/*
 * Propagate a software reset request to the real peripherals the RMU controls.
 * Only the two watchdogs are modelled today; asserting their PERI0 reset bits
 * cold-resets the linked WDT device (a no-op if no device is linked).
 */
static void k230_rmu_propagate(K230RmuState *s, hwaddr offset, uint32_t v)
{
    if (offset != K230_RMU_PERI0_CTRL) {
        return;
    }
    if ((v & K230_RMU_PERI0_WDT0_RST) && s->reset_targets[0]) {
        trace_k230_rmu_target_reset(offset, 0);
        device_cold_reset(s->reset_targets[0]);
    }
    if ((v & K230_RMU_PERI0_WDT1_RST) && s->reset_targets[1]) {
        trace_k230_rmu_target_reset(offset, 1);
        device_cold_reset(s->reset_targets[1]);
    }
}

static uint64_t k230_rmu_read(void *opaque, hwaddr offset, unsigned size)
{
    K230RmuState *s = K230_RMU(opaque);
    uint32_t value;

    if ((offset & 0x3) || offset >= K230_RMU_MMIO_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad read offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
        return 0;
    }

    /*
     * All hardware side effects happen on write, so a read just returns the
     * backing store: done bits already latched, request bits already cleared.
     */
    value = s->regs[offset / 4];
    trace_k230_rmu_read(offset, value);
    return value;
}

static void k230_rmu_write(void *opaque, hwaddr offset,
                           uint64_t val64, unsigned size)
{
    K230RmuState *s = K230_RMU(opaque);
    const K230RmuReg *r;
    uint32_t v = (uint32_t)val64;
    uint32_t old, new_val, we_gate, done_gate;
    uint32_t reset_mask = 0, done_mask = 0, persist_mask = 0, sw_mask;

    if ((offset & 0x3) || offset >= K230_RMU_MMIO_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad write offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
        return;
    }

    trace_k230_rmu_write(offset, v);
    old = s->regs[offset / 4];
    r = k230_rmu_lookup(offset);

    if (!r) {
        /*
         * Inside the 4 KiB window but not modelled: accept the write and log
         * it so unexpected accesses are visible while debugging.
         */
        qemu_log_mask(LOG_UNIMP,
                      "%s: write to unmodelled offset 0x%" HWADDR_PRIx
                      " = 0x%08x\n", __func__, offset, v);
        s->regs[offset / 4] = v;
        return;
    }

    /*
     * Derive the reset/done/persist masks for this register from its pairing
     * table. persist_mask collects reset bits that do NOT self-clear (CPU1):
     * those behave as write-enable-gated storage so software can deassert them.
     */
    for (unsigned i = 0; i < r->n_pairs; i++) {
        reset_mask |= BIT(r->pairs[i].reset_bit);
        if (r->pairs[i].done_bit != K230_RMU_NO_DONE) {
            done_mask |= BIT(r->pairs[i].done_bit);
        }
        if (!r->pairs[i].self_clear) {
            persist_mask |= BIT(r->pairs[i].reset_bit);
        }
    }

    /*
     * we_gate holds the low-half bits this write is allowed to modify. With
     * write-enable (CPU0/CPU1) a low bit n is writable only when the strobe
     * bit n+16 is also set; otherwise every low bit is directly writable.
     */
    /*
     * we_gate holds the bits this write is allowed to modify. With write-enable
     * (CPU0/CPU1) a low bit n is writable only when the strobe bit n+16 is also
     * set, so only the low half can ever change; without write-enable every bit
     * is directly writable (reserved bits are still dropped by writable_mask).
     */
    we_gate = r->has_we ? ((v >> K230_RMU_WE_SHIFT) & 0xffffu) : 0xffffffffu;

    new_val = old;

    /*
     * (A) Writable storage: bits that are neither reset nor done bits are plain
     *     read/write storage, plus any non-self-clearing reset bits (so the
     *     CPU1 request can be asserted and later deasserted). Reserved bits
     *     are dropped via writable_mask; for write-enable registers only the
     *     low half is ever reachable (we_gate is confined to 16 bits above).
     */
    sw_mask = ((~reset_mask & ~done_mask) | persist_mask)
              & we_gate & r->writable_mask;
    new_val = (new_val & ~sw_mask) | (v & sw_mask);

    /*
     * (B) Done bits are write-1-to-clear. CPU-type done bits sit in the low
     *     half and their clear is gated by write-enable (the strobe is folded
     *     into we_gate); HW_DONE done bits sit in the high half, clear direct.
     */
    done_gate = r->has_we ? (done_mask & we_gate) : done_mask;
    new_val &= ~(v & done_gate);

    /*
     * (C) A reset request latches its paired done bit immediately (zero
     *     latency). Self-clearing requests then auto-clear so they can fire
     *     again; non-self-clearing ones (CPU1) stay asserted until deasserted.
     *     FLUSH-type pairs (done_bit == NO_DONE) only auto-clear.
     */
    for (unsigned i = 0; i < r->n_pairs; i++) {
        uint32_t rbit = BIT(r->pairs[i].reset_bit);
        bool fire = (v & rbit) &&
                    (!r->has_we || (v & (rbit << K230_RMU_WE_SHIFT)));

        if (fire) {
            if (r->pairs[i].done_bit != K230_RMU_NO_DONE) {
                new_val |= BIT(r->pairs[i].done_bit);
            } else {
                trace_k230_rmu_flush(offset);
            }
            if (r->pairs[i].self_clear) {
                new_val &= ~rbit;
            }
        }
    }

    s->regs[offset / 4] = new_val;

    /* (D) Drive a real cold reset of any linked peripheral (e.g. watchdogs). */
    k230_rmu_propagate(s, offset, v);
}

static const MemoryRegionOps k230_rmu_ops = {
    .read       = k230_rmu_read,
    .write      = k230_rmu_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned       = false,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void k230_rmu_reset_hold(Object *obj, ResetType type)
{
    K230RmuState *s = K230_RMU(obj);

    trace_k230_rmu_reset_device();

    /*
     * Start from all-zero, then apply each modelled register's documented reset
     * value (K230 TRM chapter 2.1.5). Unmodelled offsets stay 0.
     */
    memset(s->regs, 0, sizeof(s->regs));
    for (size_t i = 0; i < ARRAY_SIZE(k230_rmu_regs); i++) {
        s->regs[k230_rmu_regs[i].offset / 4] = k230_rmu_regs[i].reset_val;
    }
}

static void k230_rmu_realize(DeviceState *dev, Error **errp)
{
    K230RmuState *s = K230_RMU(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &k230_rmu_ops, s,
                          TYPE_K230_RMU, K230_RMU_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);
}

static void k230_rmu_init(Object *obj)
{
    K230RmuState *s = K230_RMU(obj);

    /*
     * Optional links to the peripherals the RMU resets. The machine sets these
     * before realize; leaving one unset simply disables that reset path.
     */
    object_property_add_link(obj, "wdt0", TYPE_DEVICE,
                             (Object **)&s->reset_targets[0],
                             qdev_prop_allow_set_link_before_realize,
                             OBJ_PROP_LINK_STRONG);
    object_property_add_link(obj, "wdt1", TYPE_DEVICE,
                             (Object **)&s->reset_targets[1],
                             qdev_prop_allow_set_link_before_realize,
                             OBJ_PROP_LINK_STRONG);
}

static const VMStateDescription vmstate_k230_rmu = {
    .name               = "k230.rmu",
    .version_id         = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, K230RmuState, K230_RMU_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static void k230_rmu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = k230_rmu_realize;
    /* Resettable API: a register-clearing device needs only phases.hold. */
    rc->phases.hold = k230_rmu_reset_hold;
    dc->vmsd = &vmstate_k230_rmu;
    dc->desc = "K230 Reset Management Unit";
}

static const TypeInfo k230_rmu_info = {
    .name          = TYPE_K230_RMU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(K230RmuState),
    .instance_init = k230_rmu_init,
    .class_init    = k230_rmu_class_init,
};

static void k230_rmu_register_type(void)
{
    type_register_static(&k230_rmu_info);
}
type_init(k230_rmu_register_type)
