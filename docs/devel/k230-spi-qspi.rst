K230 SPI/QSPI Controller Implementation
=======================================

本文记录 K230 SPI/QSPI 控制器在 QEMU 中的实现方法、依据、参考代码和
验证方式。目标是能按本文从零复现当前第一版实现，而不是描述一个完整的
SPI NOR/Octal DDR 仿真器。

实现目标
--------

第一版只覆盖 K230 SDK 和 TMR 中最基础、最容易被启动链路用到的行为：

* 三个 DWC SSI 风格控制器实例：

  * QSPI0: ``0x91582000``, register window ``0x1000``, ``max-lines=4``
  * QSPI1: ``0x91583000``, register window ``0x1000``, ``max-lines=4``
  * SPI/OPI: ``0x91584000``, register window ``0x1000``, ``max-lines=8``

* SPI/OPI 实例额外挂一个只读 XIP/flash window:

  * base ``0xC0000000``
  * size ``0x08000000``

* 控制器通过 QEMU ``SSIBus`` 连接到 ``m25p80`` SPI NOR 后端。
* 默认 flash 型号使用 ``w25q256``，可通过 ``-drive if=mtd`` 绑定镜像。
* PIO ``DR0`` 字节传输可以读 ``JEDEC ID``。
* XIP window 读访问会转换成 SPI NOR read opcode/address/data 传输。

第一版明确不做：

* DMA。
* SPI NAND。
* XIP window program/erase/write。
* SPI/QSPI IRQ 到 K230 PLIC 的板级连接；当前只实现控制器内部
  ``RISR/ISR/IMR`` 状态。
* 完整的 1-4-4、4-4-4、8-8-8 物理多线电气时序。
* Octal DDR、DQS、HyperBus 精确行为。
* 性能相关 prefetch/cache 行为。

这样做的理由很简单：QEMU 的 ``SSI`` 框架是逻辑 word/byte 传输接口，
不是多根 SPI data line 的 bit-level 电气模型。控制器模型负责把寄存器配置
翻译成 opcode/address/dummy/data 字节流，再调用 ``ssi_transfer()``；
``m25p80`` 负责 SPI NOR 命令状态机。

依据
----

本次实现使用用户提供的 K230 TMR 本地副本：

``/home/flamboy/qemu-camp/docs/learning-guides/qemu-startup-param/K230_Technical_Reference_Manual_V0.3.1_20241118.pdf``

如果在其他机器复现，把上面的路径替换成同版本 TMR 的实际位置即可。

TMR 中和本实现直接相关的是 Flash Memory Controller 和 SPI 两组内容。
寄存器形态是典型 DWC SSI/DW_apb_ssi 风格，包括：

* ``CTRLR0``
* ``CTRLR1``
* ``SSIENR``
* ``SER``
* ``BAUDR``
* ``TXFTLR`` / ``RXFTLR``
* ``TXFLR`` / ``RXFLR``
* ``SR``
* ``IMR`` / ``ISR`` / ``RISR``
* ``DRx``
* ``SSIC_VERSION_ID``
* ``RX_SAMPLE_DELAY``
* ``SPI_CTRLR0``
* ``XIP_MODE_BITS``
* ``XIP_INCR_INST``
* ``XIP_WRAP_INST``
* ``XIP_CTRL``
* ``XIP_SER``
* ``XIP_CNT_TIME_OUT``
* ``SPI_CTRLR1``

实现中采用的关键固定值：

* ``CTRLR0`` reset value: ``0x00004007``
* ``SSIENR`` reset value: ``0``
* ``SSIC_VERSION_ID``: ``0x3130332a``

当前 QEMU K230 machine 的地址来自 ``hw/riscv/k230.c`` 的 ``memmap``：

.. code-block:: c

   [K230_DEV_QSPI0] = { 0x91582000, 0x00001000 },
   [K230_DEV_QSPI1] = { 0x91583000, 0x00001000 },
   [K230_DEV_SPI]   = { 0x91584000, 0x00001000 },
   [K230_DEV_FLASH] = { 0xC0000000, 0x08000000 },

SDK 侧需要确认的点是：同一组寄存器布局、三个实例地址、以及
``0xC0000000`` XIP window 绑定的实例。常见 SDK 路径中可查：

* ``src/big/rt-smart/kernel/bsp/maix3/board/interdrv/spi/drv_spi.c``
* ``src/big/rt-smart/kernel/bsp/maix3/board/board.h``

如果本机 SDK 路径不同，用下面的方式找证据：

.. code-block:: bash

   $ rg -n "dw_spi_reg_t|SSIC_VERSION_ID|SPI_CTRLR0|SPI_QOPI|SPI_OPI|SPI_XIP" /path/to/k230_sdk

本实现把 flash window 绑定到 ``0x91584000`` 的 SPI/OPI 实例。原因是 SDK
通常把 ``SPI_OPI_BASE_ADDR`` 和 ``SPI_XIP_FLASH_BASE_ADDR`` 作为一组使用；
如果后续发现某个板级配置绑定到 QSPI0，只需要在 machine 层改映射和 flash
连接，不需要重写控制器模型。

参考代码
--------

实现前应读这些现成代码：

``include/hw/ssi/ssi.h`` 和 ``hw/ssi/ssi.c``
  QEMU SPI/SSI 框架。重点是 ``ssi_create_bus()``、``ssi_transfer()``、
  ``SSI_GPIO_CS``，以及 SPI peripheral 的 CS 语义。

``hw/ssi/sifive_spi.c``
  普通 SPI master 的骨架参考。重点是 ``SysBusDevice``、``SSIBus``、
  FIFO、CS GPIO、IRQ、Meson/Kconfig 组织方式。

``hw/ssi/npcm7xx_fiu.c``
  direct flash memory window 的主要参考。它把 CPU 对 flash window 的读
  转换成：选 CS、发送 opcode、发送地址、发送 dummy、读数据、取消 CS。

``hw/block/m25p80.c``
  SPI NOR flash 后端。当前 ``w25q256`` 的 JEDEC ID 是 ``0xef4019``，
  ``JEDEC_READ`` opcode 是 ``0x9f``。控制器不应重复实现 flash 芯片状态机。

``hw/riscv/sifive_u.c`` 和 ``hw/arm/npcm7xx_boards.c``
  ``m25p80`` flash 连接方式参考，特别是 ``-drive if=mtd`` 到 flash 后端的
  绑定，以及 ``SSI_GPIO_CS`` 的连接。

``hw/watchdog/k230_wdt.c``、``include/hw/watchdog/k230_wdt.h``、
``tests/qtest/k230-wdt-test.c``
  本仓库 K230 设备模型、header 和 qtest 的本地风格参考。

文件组织
--------

新增控制器类型：

.. code-block:: text

   TYPE_K230_DW_SSI = "riscv.k230.dw-ssi"

对应文件：

.. code-block:: text

   include/hw/ssi/k230_dw_ssi.h
   hw/ssi/k230_dw_ssi.c

构建接入：

.. code-block:: text

   hw/ssi/Kconfig       -> config K230_DW_SSI, select SSI
   hw/ssi/meson.build   -> CONFIG_K230_DW_SSI builds k230_dw_ssi.c
   hw/riscv/Kconfig     -> config K230 selects K230_DW_SSI and SSI_M25P80

Machine 接入：

.. code-block:: text

   include/hw/riscv/k230.h -> K230SoCState embeds K230DwSsiState dw_ssi[3]
   hw/riscv/k230.c         -> initialize, realize, map, connect flash

测试接入：

.. code-block:: text

   tests/qtest/k230-dw-ssi-test.c
   tests/qtest/meson.build

实现步骤
--------

1. 先写 qtest，看它失败
~~~~~~~~~~~~~~~~~~~~~~~~

新增 ``tests/qtest/k230-dw-ssi-test.c``，覆盖六类行为：

* reset values: 三个实例都读 ``CTRLR0``、``SSIENR``、``SSIC_VERSION_ID``、
  ``SR``。
* register readback: 写 ``CTRLR1``、``SER``、``BAUDR``、``TXFTLR``、
  ``RXFTLR``、``IMR``、``SPI_CTRLR0``、``XIP_INCR_INST`` 后读回。
* JEDEC ID: 通过 ``0x91584000 + DR0`` 发送 ``0x9f`` 和三个 dummy byte，
  期望读到 ``0xef 0x40 0x19``。
* disabled/no-CS DR write: controller 未使能或未选 CS 时写 ``DR0`` 不产生
  无法消费的 stale TX FIFO 数据。
* internal interrupt status: 覆盖 ``RXFTLR``、``IMR``、``RISR``、``ISR`` 的
  控制器内部状态联动。第一版不测 PLIC pending，因为 machine 还没有
  SPI/QSPI IRQ 号证据。
* XIP window read: 创建临时 ``w25q256`` raw image，在 offset ``0x100``
  写 pattern，然后从 ``0xC0000100`` 用 byte/word/long/quad 读回；再设置
  ``XIP_INCR_INST=0x13``，从 ``0xC1000100`` 覆盖 READ4/4-byte address。

把测试加入 ``tests/qtest/meson.build`` 的 ``CONFIG_K230`` riscv64 qtest 列表。

运行：

.. code-block:: bash

   $ build/pyvenv/bin/meson test -C build qtest-riscv64/k230-dw-ssi-test

在控制器未实现时，测试应失败。例如 ``CTRLR0`` 会从 unimplemented device
读到 ``0``，而不是 ``0x00004007``。这一步证明测试确实覆盖了缺失行为。

2. 定义设备状态
~~~~~~~~~~~~~~~~

``include/hw/ssi/k230_dw_ssi.h`` 中定义 ``K230DwSsiState``：

.. code-block:: c

   typedef struct K230DwSsiXip {
       MemoryRegion mmio;
       bool enabled;
       hwaddr window_size;
   } K230DwSsiXip;

   struct K230DwSsiState {
       SysBusDevice parent_obj;

       MemoryRegion mmio;
       K230DwSsiXip xip;
       SSIBus *spi;
       qemu_irq irq;
       qemu_irq *cs_lines;

       Fifo8 tx_fifo;
       Fifo8 rx_fifo;
       uint32_t regs[K230_DW_SSI_NUM_REGS];

       uint32_t num_cs;
       uint32_t max_lines;
       int active_cs;
   };

职责划分：

* ``mmio`` 是控制器寄存器 window。
* ``xip`` 汇总可选 XIP window 的 MemoryRegion、开关和窗口大小。
* ``spi`` 是 QEMU ``SSIBus``，连接 ``m25p80``。
* ``cs_lines`` 是输出 GPIO，低电平选中 flash。
* ``regs`` 保存普通寄存器读回值。
* ``tx_fifo``/``rx_fifo`` 支撑 PIO DRx 行为。

控制器对外保持 ``0x1000`` 大小的 MMIO window，``regs`` 只保存
``0x000..0x118`` 范围内的 71 个寄存器项。
* ``max_lines`` 区分 QSPI 和 OPI，但第一版不模拟真实多线电气时序。

3. 定义寄存器 offset
~~~~~~~~~~~~~~~~~~~~

在 header 里按 TMR/DWC SSI 布局定义 offset。第一版需要这些 offset：

.. code-block:: c

   K230_DW_SSI_CTRLR0          = 0x000,
   K230_DW_SSI_CTRLR1          = 0x004,
   K230_DW_SSI_SSIENR          = 0x008,
   K230_DW_SSI_SER             = 0x010,
   K230_DW_SSI_BAUDR           = 0x014,
   K230_DW_SSI_TXFTLR          = 0x018,
   K230_DW_SSI_RXFTLR          = 0x01c,
   K230_DW_SSI_TXFLR           = 0x020,
   K230_DW_SSI_RXFLR           = 0x024,
   K230_DW_SSI_SR              = 0x028,
   K230_DW_SSI_IMR             = 0x02c,
   K230_DW_SSI_ISR             = 0x030,
   K230_DW_SSI_RISR            = 0x034,
   K230_DW_SSI_DR0             = 0x060,
   K230_DW_SSI_DR_END          = 0x0ec,
   K230_DW_SSI_RX_SAMPLE_DELAY = 0x0f0,
   K230_DW_SSI_SPI_CTRLR0      = 0x0f4,
   K230_DW_SSI_XIP_INCR_INST   = 0x100,
   K230_DW_SSI_SPI_CTRLR1      = 0x118,

4. 实现 MMIO read/write
~~~~~~~~~~~~~~~~~~~~~~~

控制器寄存器用 little-endian 32-bit access：

.. code-block:: c

   static const MemoryRegionOps k230_dw_ssi_ops = {
       .read = k230_dw_ssi_read,
       .write = k230_dw_ssi_write,
       .endianness = DEVICE_LITTLE_ENDIAN,
       .impl = {
           .min_access_size = 4,
           .max_access_size = 4,
           .unaligned = false,
       },
   };

读路径：

* ``DR0..DR_END``：从 ``rx_fifo`` 弹一个 byte，没有数据则返回 ``0``。
* ``TXFLR``：返回 ``tx_fifo`` 使用量。
* ``RXFLR``：返回 ``rx_fifo`` 使用量。
* ``SR``：返回 ``TFNF``、``TFE``、``RFNE``、``RFF`` 等最小状态位。
* ``RISR``：返回 raw FIFO interrupt 状态。
* ``ISR``：返回 ``RISR & IMR``。
* interrupt clear registers：读返回 ``0``。
* ``SSIC_VERSION_ID``：返回 ``0x3130332a``。
* 其他实现寄存器：返回 ``regs[]`` 保存值。

写路径：

* ``DR0..DR_END``：低 8 bit 作为一个 SSI byte 发送。
* ``SER``：按 ``num-cs`` mask 后保存，并更新 CS。
* ``SSIENR``：只保留 enable bit，并更新 CS/IRQ。
* 普通配置寄存器：保存写入值，满足 SDK/driver 写后读回和后续 XIP 配置。
* 只读/clear/status 寄存器：忽略写入。
* 越界或非 32-bit 对齐 offset：用 ``qemu_log_mask(LOG_GUEST_ERROR, ...)``
  记录 guest 错误。

5. 实现 CS 和 PIO transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~

CS 语义参考 ``npcm7xx_fiu.c``：

* ``qemu_irq_lower(cs)`` 表示选中。
* ``qemu_irq_raise(cs)`` 表示取消选中。

``SER`` 和 ``SSIENR`` 同时决定活动 CS：

* controller disabled 或 ``SER == 0`` 时取消 CS。
* controller enabled 且 ``SER`` 有 bit 时，选择最低 set bit 对应的 CS。

PIO ``DR0`` 写入时：

* controller disabled 或未选 CS：直接忽略这次写入，不访问 flash，也不缓存到
  ``tx_fifo``。
* controller enabled 且 CS active：调用 ``ssi_transfer(s->spi, tx)``，
  把返回 byte 推入 ``rx_fifo``。

这个模型是“立即传输”而不是完整 TX FIFO drain engine。既然第一版没有 drain
engine，就不保留未激活传输时写入的 TX 数据，避免 ``TXFLR/SR`` 留下后续无法
消费的 stale FIFO 状态。它足够覆盖 polling PIO 和 JEDEC/read 类命令，同时保持
第一版简单。

6. 实现 XIP/flash window
~~~~~~~~~~~~~~~~~~~~~~~~

只有 SPI/OPI 实例设置 ``has-xip=true`` 时初始化第二个 MMIO region：

.. code-block:: c

   memory_region_init_io(&s->xip.mmio, OBJECT(s),
                         &k230_dw_ssi_xip_ops, s,
                         "k230.dw-ssi.xip", s->xip.window_size);
   sysbus_init_mmio(sbd, &s->xip.mmio);

XIP read 流程：

.. code-block:: text

   CPU load 0xC0000100
     -> k230_dw_ssi_xip_read(addr=0x100, size=N)
     -> force a CS edge by deselecting any active CS first
     -> select CS0
     -> send opcode from XIP_INCR_INST, default 0x03
     -> send 24-bit or 32-bit address, depending on read opcode
     -> send dummy bytes for FAST/Quad read opcodes
     -> read N bytes through ssi_transfer(..., 0)
     -> little-endian pack into uint64_t
     -> deselect CS0

第一版默认 opcode 是 ``0x03``，这是 ``m25p80`` 已支持的普通 READ。
``0x13``、``0x0c``、``0x3c``、``0x6c``、``0xbc``、``0xec`` 按 4-byte
address 处理，其他 read opcode 默认按 3-byte address 处理。``0x0b``、
``0x6b``、``0xeb`` 这类 fast/quad read opcode 先按一个 dummy byte 处理。
后续如果 SDK 需要更精确 dummy cycle，可在这里按 ``SPI_CTRLR0`` 字段补解析。

XIP write 第一版只记录 guest error，不修改 flash。flash program/erase 应该先
通过 ``m25p80`` 的标准命令路径建测试，再决定是否开放。

7. 实现 reset、instance_init、realize、vmstate、properties
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

reset：

* 清空 ``regs``、``tx_fifo``、``rx_fifo``。
* 设置 ``CTRLR0 = 0x00004007``。
* 设置 ``SSIC_VERSION_ID = 0x3130332a``。
* 设置 ``BAUDR = 2``。
* 设置 ``XIP_INCR_INST = 0x03``。
* 取消所有 CS。
* 更新 IRQ。

instance_init：

* ``ssi_create_bus(dev, "spi")``。
* ``sysbus_init_irq()``。
* 初始化控制寄存器 ``mmio`` MemoryRegion。
* 创建 FIFO。

realize：

* 校验 ``num-cs`` 为 ``1..8``。
* 校验 ``max-lines`` 为 ``1``、``4`` 或 ``8``。
* 如果 ``has-xip``，要求 ``flash-window-size != 0``。
* ``qdev_init_gpio_out_named(dev, s->cs_lines, "cs", s->num_cs)``。
* 初始化可选 ``xip`` MemoryRegion。

properties：

.. code-block:: c

   DEFINE_PROP_UINT32("num-cs", K230DwSsiState, num_cs, 1),
   DEFINE_PROP_UINT32("max-lines", K230DwSsiState, max_lines, 1),
   DEFINE_PROP_BOOL("has-xip", K230DwSsiState, xip.enabled, false),
   DEFINE_PROP_SIZE("flash-window-size", K230DwSsiState, xip.window_size, 0),

8. 接入 K230 machine
~~~~~~~~~~~~~~~~~~~~

``include/hw/riscv/k230.h``：

.. code-block:: c

   #include "hw/ssi/k230_dw_ssi.h"

   K230DwSsiState dw_ssi[3];

``hw/riscv/k230.c`` 初始化三个 child：

.. code-block:: c

   object_initialize_child(obj, "k230-qspi0", &s->dw_ssi[0],
                           TYPE_K230_DW_SSI);
   object_initialize_child(obj, "k230-qspi1", &s->dw_ssi[1],
                           TYPE_K230_DW_SSI);
   object_initialize_child(obj, "k230-spi-opi", &s->dw_ssi[2],
                           TYPE_K230_DW_SSI);

realize 前设置实例差异：

.. code-block:: c

   qdev_prop_set_uint32(DEVICE(&s->dw_ssi[0]), "max-lines", 4);
   qdev_prop_set_uint32(DEVICE(&s->dw_ssi[1]), "max-lines", 4);
   qdev_prop_set_uint32(DEVICE(&s->dw_ssi[2]), "max-lines", 8);
   qdev_prop_set_bit(DEVICE(&s->dw_ssi[2]), "has-xip", true);
   qdev_prop_set_uint64(DEVICE(&s->dw_ssi[2]), "flash-window-size",
                        memmap[K230_DEV_FLASH].size);

map：

.. code-block:: c

   sysbus_mmio_map(SYS_BUS_DEVICE(&s->dw_ssi[0]), 0,
                   memmap[K230_DEV_QSPI0].base);
   sysbus_mmio_map(SYS_BUS_DEVICE(&s->dw_ssi[1]), 0,
                   memmap[K230_DEV_QSPI1].base);
   sysbus_mmio_map(SYS_BUS_DEVICE(&s->dw_ssi[2]), 0,
                   memmap[K230_DEV_SPI].base);
   sysbus_mmio_map(SYS_BUS_DEVICE(&s->dw_ssi[2]), 1,
                   memmap[K230_DEV_FLASH].base);

flash 连接：

.. code-block:: c

   static void k230_connect_spi_flash(K230DwSsiState *ssi,
                                      const char *flash_type,
                                      DriveInfo *dinfo)
   {
       DeviceState *flash = qdev_new(flash_type);
       qemu_irq flash_cs;

       if (dinfo) {
           qdev_prop_set_drive_err(flash, "drive",
                                   blk_by_legacy_dinfo(dinfo), &error_fatal);
       }

       qdev_realize_and_unref(flash, BUS(ssi->spi), &error_fatal);
       flash_cs = qdev_get_gpio_in_named(flash, SSI_GPIO_CS, 0);
       qdev_connect_gpio_out_named(DEVICE(ssi), "cs", 0, flash_cs);
   }

   k230_connect_spi_flash(&s->dw_ssi[2], "w25q256",
                          drive_get(IF_MTD, 0, 0));

最后删除原来的 ``qspi0``、``qspi1``、``spi``、``flash`` unimplemented 占位。

9. 验证
~~~~~~~

最小验证：

.. code-block:: bash

   $ build/pyvenv/bin/meson test -C build qtest-riscv64/k230-dw-ssi-test
   $ build/pyvenv/bin/meson test -C build qtest-riscv64/k230-wdt-test

当前实现通过：

.. code-block:: text

   qtest-riscv64/k230-dw-ssi-test: 6 subtests passed
   qtest-riscv64/k230-wdt-test:    7 subtests passed

如果测试失败，优先按失败类型定位：

* reset value 错：检查 ``k230_dw_ssi_reset()`` 和 machine 是否还保留
  unimplemented overlap。
* register readback 错：检查 ``k230_dw_ssi_write()`` 是否把对应 offset 归类为
  普通配置寄存器。
* JEDEC ID 错：检查 ``SER``/``SSIENR`` 顺序、CS 是否低电平选中、``m25p80``
  是否连接到 ``s->dw_ssi[2].spi``。
* XIP 读错：检查第二个 MMIO region 是否映射到 ``0xC0000000``，opcode 是否为
  ``0x03`` 或 ``0x13``，地址字节顺序是否为 big-endian SPI address，以及
  READ4 opcode 是否发送 4 个地址字节。

后续扩展
--------

需要跑 U-Boot/Linux 或更完整 SDK 驱动时，再逐项补：

* ``SPI_CTRLR0`` 字段解析，包括 address length、transfer type、dummy cycles。
* SPI/QSPI IRQ 到 PLIC 的连接。做这一步前应先从 TMR/SDK 确认 QSPI0、
  QSPI1、SPI/OPI 对应 PLIC source ID，再补 ``include/hw/riscv/k230.h`` 常量、
  ``sysbus_connect_irq()`` 和 PLIC pending qtest。
* 中断清除寄存器的精确 latch/clear 行为。
* TX FIFO drain engine，而不是当前的 DR 写立即传输。
* 多 CS flash 或非 flash SPI peripheral。
* write enable、program、erase 的控制器路径测试。
* Octal DDR/DQS 行为。只有真实 guest 依赖时再做，避免过度设计。
