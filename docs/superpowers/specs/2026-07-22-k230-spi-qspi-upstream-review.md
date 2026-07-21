# K230 SPI/QSPI 上游自审规则文档

> 本文件是「裁判规则」：由规则制定者定标准，由提交者（你）逐条自审。
> 目标对象：K230 DesignWare SSI 兼容 SPI/QSPI 控制器建模及其 SoC 集成层，准备以约 10 个 patch 推送 QEMU 上游。
> 生成日期：2026-07-22

---

## 0. 如何使用本文档

1. **范围**：核心是 `hw/ssi/k230_dw_ssi.c` / `include/hw/ssi/k230_dw_ssi.h` 及其 qtest；集成层覆盖 `hw/riscv/k230.c`、`hw/misc/k230_hi_sys.*`、`tests/qtest/k230-dw-ssi-test.c`、`docs/system/riscv/k230.rst`、`meson.build`/`Kconfig`、`MAINTAINERS`。watchdog 与机器启动内部细节不在深审范围，仅作集成触点标注。
2. **严重度**：
   - 🔴 **Blocker** — 必改，否则 maintainer 直接打回或 CI 红。
   - 🟡 **Major** — 强烈建议，影响正确性、可维护性或上游接纳概率。
   - 🟢 **Minor** — 锦上添花。
3. **每条条目格式**：
   - `[ ]` 勾选框
   - **判定**：什么算通过
   - **为什么**：上游为什么在意
   - **怎么验证**：具体命令/检查动作
4. **自审流程建议**：先扫全部 🔴 → 再扫 🟡 → 最后 🟢。每域末尾有「域级出口标准」。

---

## 1. 进程卫生（Process & Submission）

### 1.1 DCO 与签名
- [ ] 🔴 **每个 commit 含 `Signed-off-by:` 行**，且与 author/committer 身份一致。
  - 判定：`git log --format='%an %ae | %s'` 的作者 = `Signed-off-by` 的邮箱。
  - 为什么：QEMU 强制 DCO，`scripts/checkpatch.pl` 与 GitLab CI `.gitlab-ci.d/check-dco.py` 会拦截。
  - 怎么验证：`git log -<n> --pretty=fuller | grep -i signed-off-by`；`python .gitlab-ci.d/check-dco.py`。

- [ ] 🔴 **无 `Developed-by:`/`Co-authored-by:` 等无签名 trail 混入**，除非确有第三方贡献并按 QEMU 规范完整署名。

### 1.2 Commit 信息
- [ ] 🔴 **commit 前缀使用正确子系统**：`hw/ssi:`、`hw/riscv:`、`hw/misc:`、`tests/qtest:`、`docs:`、`MAINTAINERS:`。前缀小写、冒号后空格。
  - 为什么：QEMU `checkpatch` 与 maintainer 按 prefix 路由。
  - 怎么验证：`git log --oneline -<n>` 逐条对照；`scripts/checkpatch.pl --strict` 对每条 message。

- [ ] 🟡 **每个 commit 的 subject ≤ 75 字符**，body 用 72 列折行，空行分隔 subject 与 body。
- [ ] 🟡 **cover letter（`git publish`/b4）存在**，说明 series 目的、依赖、测试结果、如何运行 qtest。0/N patch。
- [ ] 🟡 **`MAINTAINERS` 入口在 series 内**：`K230 Machines` 段覆盖所有新增 `F:` 路径，邮箱/状态正确。
  - 怎么验证：`scripts/get_maintainer.pl --git <某新增文件>` 应输出你。

### 1.3 checkpatch / 静态检查
- [ ] 🔴 **每个 patch 通过 `scripts/checkpatch.pl --strict --codespell` 且无 ERROR/WARNING**。
  - 怎么验证：对每个 `.patch` 跑一次；CI：`.gitlab-ci.d/check-patch.py`、`check-units.py`。

- [ ] 🔴 **通过 `scripts/clean-header-guards.pl` / `clean-includes`**：头文件 guard 唯一、无多余 include。

### 1.4 可 bisect
- [ ] 🔴 **每个 commit 单独可编译且不破坏 `make check-qtest-riscv64`**。
  - 为什么：maintainer 强制 bisect 干净；半成品控制器接入会致 board 启动崩溃。
  - 怎么验证：`for c in $(git rev-list <base>..HEAD --reverse); do git checkout $c; ninja -C build && ...; done`。

- [ ] 🟡 **依赖顺序合理**：先 `k230_hi_sys` 再 `k230_dw_ssi` 再 SoC 装配再文档/MAINTAINERS，避免前向引用。

---

## 2. 许可证与文件头

- [ ] 🔴 **每个新增 `.c/.h` 顶部含 `SPDX-License-Identifier: GPL-2.0-or-later`**，与 QEMU 主体一致。
  - 为什么：无 SPDX 的文件会被 checkpatch 与 license 扫描打回。
  - 怎么验证：`grep -L SPDX-License-Identifier <files>` 应为空。

- [ ] 🔴 **头文件 guard 形如 `HW_SSI_K230_DW_SSI_H`**，全大写、与路径对应、唯一（全局无重复）。
- [ ] 🟢 **`.c` 顶部有简短功能说明注释**（< 6 行），引用 K230 TRM 章节/版本号；不要堆砌版权年份模板。

---

## 3. 构建系统（Meson / Kconfig）

### 3.1 Kconfig
- [ ] 🔴 **`hw/ssi/Kconfig` 新增 `config K230_DW_SSI` 项**，`select SSI`（或正确依赖），`default y` 仅在对应 RISC-V 板子选。
  - 怎么验证：`scripts/minikconf.py` 解析无环、无未定义符号。

- [ ] 🟡 **K230 machine 的 `Kconfig` `select K230_DW_SSI`**，而非在 `meson` 里硬连。
- [ ] 🟡 **`hw/misc/k230_hi_sys` 同样新增 Kconfig 项并被 SoC select**。

### 3.2 meson.build
- [ ] 🔴 **`hw/ssi/meson.build` 用 `system_ss.add(when: 'K230_DW_SSI', ...)` 注册源文件**，不放在无条件的 `system_ss` 里。
  - 为什么：上游要求按 Kconfig 门控，否则全平台编进。
  - 怎么验证：`grep -n K230_DW_SSI hw/ssi/meson.build`。

- [ ] 🟡 **`tests/qtest/meson.build` 注册 `k230-dw-ssi-test`**，且 `depends` 含正确 target（`riscv64-softmmu`），qtest 列表与现有风格一致。

- [ ] 🟢 **无多余 `subdir_done()` / 硬编码路径**。

---

## 4. QOM 与设备生命周期

### 4.1 类型定义
- [ ] 🔴 **`TYPE_K230_DW_SSI` = `"riscv.k230.dw-ssi"`**，`OBJECT_DECLARE_SIMPLE_TYPE` + `OBJECT_CHECK` 一致；`parent = TYPE_SYS_BUS_DEVICE`。
- [ ] 🔴 **`instance_size`/`instance_init`/`instance_finalize`/`class_init`/`type_init` 齐全**；`type_register_static` 而非运行期注册。

### 4.2 realize
- [ ] 🔴 **`realize` 用 `Error **errp` + `error_setg` + `return`**，绝不 `exit(1)`/`abort()`。
  - 判定：`realize` 失败路径返回且设置 errp。
  - 为什么：QEMU 上游硬规则。
  - 怎么验证：读 `k230_dw_ssi_realize`，检查 `num_cs`/`max_lines` 校验。

- [ ] 🔴 **`realize` 中分配的资源（`g_new0(cs_lines)`）在 `instance_finalize` 释放**；`fifo32_create` ↔ `fifo32_destroy` 配对。
- [ ] 🟡 **`qdev_init_gpio_out_named(dev, cs_lines, "cs", num_cs)`** 在 realize 内（不在 init）调用；GPIO 在 realize 后才可用。
- [ ] 🟡 **`num_cs` 范围校验 1..8、`max_lines ∈ {1,4,8}`** 已实现；非法值报错并 return。

### 4.3 Resettable 三阶段
- [ ] 🔴 **实现 `ResettableClass` 的 `enter`/`hold`/`exit` 三阶段**，而非旧 `DeviceClass::reset`。
  - 判定：`rc->phases.enter`/`hold`/`exit` 均赋值。
  - 为什么：QEMU 已迁移到三阶段 reset；旧接口对新设备会触发弃用告警。

- [ ] 🔴 **`enter`：清空所有寄存器/FIFO/相位/状态，再写复位值**；`hold`：操作对外引脚（拉高 cs_lines）；`exit`：重算并更新 IRQ。
  - 判定：读 `k230_dw_ssi_enter_reset`/`hold_reset`/`exit_reset`，确认与上面一致。
  - 怎么验证：qtest 复位后读 `CTRLR0/IMR/IDR/SSIC_VERSION_ID/SPI_CTRLR0` 与定义的 `*_RESET` 宏一致。

- [ ] 🟡 **`max_lines == 8` 时 `SPI_CTRLR0` 复位值走 `FMC_RESET`，否则 `SPI_RESET`** 已实现（影响 flash 控制器实例）。
- [ ] 🟡 **复位后 `active_cs = -1`、所有 cs_lines raise**。

### 4.4 属性与 vmstate
- [ ] 🟡 **`num-cs`、`max-lines` 用 `DEFINE_PROP_UINT32` + `Property[]`** 注册，有默认值（1 / 1）。
- [ ] 🔴 **`VMStateDescription` 覆盖全部运行期可见状态**：`regs[]`、`tx_fifo`/`rx_fifo`（`VMSTATE_FIFO32`）、`irq_latched`、`idma_completed_frames`、`phase`、`remaining_frames`、`enhanced.*`（所有子字段含 `mode_bits_enabled` 的 `BOOL`）、`active_cs`、`sleep_status`。
  - 判定：struct 里每个非派生字段都在 vmsd 里。
  - 为什么：漏字段致迁移/快照破坏；上游 CI 有 `vmstate-static-checker`。
  - 怎么验证：对照 `struct K230DwSsiState` 逐字段核对 `.fields`。

- [ ] 🔴 **`post_load` 调用 `k230_dw_ssi_update_irq`** 重算电平（已有）。
- [ ] 🟡 **vmsd 无 `version_id` 不兼容**：未设 `minimum_version_id` 时默认从 1 开始，新增字段需谨慎；如复用现有结构，确认 `version_id` 语义。

---

## 5. 寄存器建模

### 5.1 宏与字段
- [ ] 🔴 **所有寄存器用 `REG32(NAME, offset)` + `FIELD(...)`** 定义，禁止裸魔数。
- [ ] 🔴 **`A_*`（地址常量）与 `R_*`（索引常量）使用一致**：读写在 `regs[addr/sizeof(uint32_t)]` 时用 `R_*`，case 用 `A_*`。
- [ ] 🟡 **`K230_DW_SSI_REGS_SIZE = 0x14c` 与最后一个 `REG32` 偏移 +4 对齐**，`NUM_REGS` 计算正确。

### 5.2 复位值
- [ ] 🔴 **每个有非零复位值的寄存器，复位后等于 TRM 定义值**：`CTRLR0/SR/IMR/IDR/SSIC_VERSION_ID/SPI_CTRLR0/AXIAWLEN/AXIARLEN`。
  - 怎么验证：qtest 复位后逐个读，对照宏。

### 5.3 可写掩码与写规则
- [ ] 🔴 **每个可写寄存器有 `*_WRITABLE_MASK`**，写通过 `k230_dw_ssi_write_masked` 而非直接赋值；保留位不写入。
  - 怎么验证：grep 所有 `case A_*:` 写路径，确认都经 `write_masked`。

- [ ] 🔴 **读清（read-to-clear）寄存器走 `k230_dw_ssi_irq_read_clear`**：`TXEICR/RXOICR/RXUICR/MSTICR/ICR/AXIECR/DONECR`；读动作返回是否有 active 并清 latch。
- [ ] 🟡 **RAZWI 寄存器（`XIP_CTRL/XIP_SER/XRXOICR/...`）读返 0、写忽略、不报错**（已有 `is_razwi`）。
- [ ] 🟡 **写即触发错误日志的只读寄存器（`TXFLR/RXFLR/SR/ISR/RISR/ICR*`）写为 no-op 且不打 LOG_GUEST_ERROR**（已有空 case）。

### 5.4 写已启用寄存器拒绝
- [ ] 🔴 **`CTRLR0/CTRLR1/MWCR/BAUDR/SPI_CTRLR0` 在 `SSIENR==1` 时写被拒并打 `LOG_GUEST_ERROR`**（已有 `write_requires_disabled`）。
  - 为什么：DWC SSI 硬件约束；上游审阅会盯这条。
  - 怎么验证：qtest 先 enable 再写这些寄存器，读回应不变。

### 5.5 访问尺寸与坏偏移
- [ ] 🔴 **`MemoryRegionOps.impl`/`.valid` 限定 4 字节对齐**（mmio）；DR 区允许 4 字节；坏偏移打 `LOG_GUEST_ERROR`。
  - 怎么验证：读 `k230_dw_ssi_ops`；qtest 用 1/2/8 字节访问应被拒或合并。

- [ ] 🟡 **非对齐访问行为明确**（`unaligned = false`）：与 TRM 一致。

---

## 6. 内存区（MemoryRegion）

- [ ] 🔴 **两个独立 `MemoryRegion`**：`mmio`（控制寄存器，4K）与 `xip`（XIP 窗口，128M）。
  - 判定：`sysbus_init_mmio` 调用两次；`K230_DW_SSI_MMIO_SIZE=0x1000`、`XIP_WINDOW_SIZE=0x08000000`。
- [ ] 🟡 **`xip` 区 ops 的 `impl.min/max_access_size=1..8` 且 `unaligned=true`**，因 XIP 可按字节读（已有）。
- [ ] 🟡 **`mmio` 与 `xip` 的名字（`TYPE_K230_DW_SSI` / `.xip`）唯一**，便于 trace 与 memory map 调试。
- [ ] 🟢 **`endianness = DEVICE_LITTLE_ENDIAN`** 两处一致。

---

## 7. SPI 总线与片选

- [ ] 🔴 **`ssi_create_bus(dev, "spi")`** 在 `instance_init`，总线名 `"spi"`。
- [ ] 🔴 **`num_cs` 个 cs_lines 输出 GPIO**，`SER` 寄存器按 `num_cs` 掩码；多选/越界打 `LOG_GUEST_ERROR`。
  - 判定：`k230_dw_ssi_update_cs` 检测 `ser & (ser-1)` 报错。
  - 怎么验证：qtest 写 `SER=0x3` 应报错且不选 CS。

- [ ] 🟡 **CS 极性**：空闲 raise、选中 lower；`active_cs` 跟踪当前选中；`deselect` 在 `active_cs<0` 时早退。
- [ ] 🟡 **`SER` 清零触发 `abort_transfer`**（已有）；`SSIENR` 清零触发 abort + `sleep_status=true`。
- [ ] 🟡 **`max_lines` 强制 SPI_FRF 线数上限**：`SPI_FRF=1` 需 2 线、`=2` 需 4 线；不足打 `LOG_GUEST_ERROR` 拒绝。
  - 判定：`k230_dw_ssi_enhanced_config_supported`/`xip_config_supported` 检查 `required_lines > max_lines`。

---

## 8. 传输引擎 —— 标准 SPI（TMOD）

- [ ] 🔴 **四种 TMOD 全覆盖**：`TR`（收发）、`TO`（仅发）、`RO`（仅收）、`EEPROM_READ`。
  - 判定：`k230_dw_ssi_run_transfer` 的 switch 4 分支齐全。
  - 怎么验证：qtest 各跑一轮，校验 RX FIFO 内容与 NDF。

- [ ] 🔴 **`DFS` 帧掩码**：`k230_dw_ssi_frame_masked` 按 `DFS+1` 位掩码，32 位特判 `UINT32_MAX`；TX 与 RX 都应用掩码。
- [ ] 🔴 **`SRL`（回环）走 `rx=tx` 不调用 `ssi_transfer`**（已有）。
- [ ] 🟡 **`TO` 模式批处理（`PIO_TX_BATCH=64`）防死循环**：循环上限 + `phase=STANDARD_TX_ONLY`，读 `TXFLR/SR` 时推进剩余批次。
  - 为什么：TO 模式无限 TX 会让 QEMU 主循环饿死。
  - 怎么验证：写超 64 帧后读 SR，确认 BUSY 持续但 QEMU 不挂。

- [ ] 🟡 **`RO`/`EEPROM_READ` 的 `NDF+1` 帧**计数正确，`remaining_frames` 归零回 IDLE。
- [ ] 🟡 **RX FIFO 满时**：`TR` 模式 break 并打 `LOG_GUEST_ERROR` + 置 `RXOIR`；不静默丢帧。

---

## 9. 增强模式 / QSPI（Dual/Quad）

- [ ] 🔴 **`SPI_FRF ∈ {1,2}` 触发增强路径**，`=0` 走标准；非法值打 `LOG_GUEST_ERROR` 返回。
- [ ] 🔴 **`TRANS_TYPE ∈ {0,1,2}` 校验**，`>2` 报错拒绝（已有）。
- [ ] 🔴 **指令长度 `INST_L`→`1<<(inst_l+1)` 位，0 表示无指令**；地址长度 `ADDR_L<<2` 位，`>32` 报错。
- [ ] 🔴 **`WAIT_CYCLES` dummy 字节数按 `trans_type` 与 `spi_frf` 线数换算**（`k230_dw_ssi_dummy_bytes`）；trans_type=0 dummy 走单线，否则按 dual/quad 线数。
  - 怎么验证：qtest 设 trans_type=0/1/2 × spi_frf=1/2，校验 ssi 总线上 dummy byte 数。

- [ ] 🔴 **DDR / RXDS 显式不支持**：`SPI_DDR_EN`/`INST_DDR_EN`/`SPI_RXDS_EN`/`SPI_RXDS_SIG_EN` 任一置位即 `LOG_GUEST_ERROR` 返回 false。
  - 为什么：上游要求「未实现特性必须显式拒绝而非静默错误行为」。

- [ ] 🟡 **增强模式仅支持 `TMOD_RO`/`TMOD_TO`**，其他报错（已有）。
- [ ] 🟡 **mode bits**：`XIP_MD_BIT_EN` 时长度 `1<<(XIP_MBL+1)`，从 `XIP_MODE_BITS` 取并掩码。
- [ ] 🟡 **相位机**：`INSTRUCTION→ADDRESS→MODE→DUMMY→DATA` 逐相位推进，`remaining_frames` 归零回 IDLE；`g_assert_not_reached()` 守非法相位。

---

## 10. XIP（Execute-in-Place）

- [ ] 🔴 **XIP 读路径经 `k230_dw_ssi_xip_read`**，受 `hi_sys` 门控（`k230_hi_sys_xip_enabled`）；未使能返 0。
  - 判定：`if (!s->hi_sys || !k230_hi_sys_xip_enabled(s->hi_sys)) return 0;`
  - 为什么：K230 XIP 由 hi_sys 全局使能；漏门控致误触发。

- [ ] 🔴 **XIP 序列**：instruction → address → mode（若 en）→ dummy bytes → data，与增强模式一致。
- [ ] 🔴 **XIP 写拒绝**：`xip_write` 打 `LOG_GUEST_ERROR` 并丢弃（已有）。
- [ ] 🟡 **XIP 读尺寸 1..8 字节，小端拼接**：`value |= (byte) << (8*i)`；与 `endianness=LITTLE` 一致。
  - 怎么验证：qtest XIP 读 4 字节，校验字节序与 flash 内容。

- [ ] 🟡 **XIP 期间若控制器忙则 `abort_transfer` 后重选 CS 0**（已有）；`active_cs != 0` 时返 0。
- [ ] 🟡 **XIP 不依赖 TX/RX FIFO**，直接 `ssi_transfer`；用完 deselect。
- [ ] 🟢 **XIP 配置支持性检查**（`xip_config_supported`）覆盖 `SPI_FRF=0/1/2` 三档，`>2` 报错。

---

## 11. IDMA（Indirect DMA）

- [ ] 🔴 **IDMA 使能条件完整**：`IDMAE && AINC && SSIENR && SER 单比特 && phase==IDLE`（`k230_dw_ssi_idma_ready`）。
  - 怎么验证：qtest 逐个条件缺失时不触发 IDMA。

- [ ] 🔴 **`AINC=0`（固定地址）打 `LOG_UNIMP`**，不静默（已有）。
- [ ] 🔴 **DFS 必须为 7（8-bit）**，否则 `LOG_UNIMP` + 结束（已有）；上游会查此限制是否合理。
- [ ] 🔴 **64 位地址**：`AXIAR0 | (AXIAR1<<32)`；地址 + length 溢出检查（`address > UINT64_MAX - (length-1)`）。
  - 为什么：溢出致 `dma_memory_*` 越界；上游安全审会盯。

- [ ] 🔴 **`dma_memory_read/write` 返回值检查 `MEMTX_OK`**，失败打 `LOG_GUEST_ERROR` + 置 `AXIER` + 结束（已有 `idma_fail`）。
- [ ] 🔴 **长度 `NDF+1`** 与 buffer 分配一致；`g_malloc(length)`，length=0 安全（`g_malloc(0)` 返回非 NULL，但建议 length>0 校验）。
  - 🟡 建议：length==0 时跳过 DMA 直接 DONE。

- [ ] 🔴 **IDMA 结束**：清 `SSIENR`、回 IDLE、`remaining_frames=0`、deselect、置 `DONE`/`AXIE` latch、更新 IRQ（`idma_end`）。
- [ ] 🟡 **`TMOD_RO` 读 flash → `dma_memory_write` 回内存**；`TMOD_TO` 读内存 → 发 flash；方向正确。
- [ ] 🟡 **`trans_type==1 && wait_cycles>=2` 的 mode bits 特殊处理**（已有，从 `XIP_MODE_BITS` 取 8 位、扣 2 cycles）；与 TRM 一致。
- [ ] 🟡 **`idma_completed_frames` 用于 `SR.CMPLTD_DF`**；结束后清零。

---

## 12. 中断

### 12.1 模型
- [ ] 🔴 **三态**：`irq_latched`（锁存的边沿事件）+ 实时计算（`TXEIR`/`RXFIR` 按阈值）= `irq_raw_status`；`ISR = raw & IMR`，`RISR = raw`。
  - 判定：`k230_dw_ssi_irq_raw_status` 合并 `irq_latched` 与 FIFO 阈值位。
  - 怎么验证：qtest 设阈值、填/排 FIFO，读 ISR/RISR。

- [ ] 🔴 **`K230_DW_SSI_IRQ_VALID_MASK = 0x9bf`** 屏蔽未实现位，raw/ISR/RISR 全程 & 此掩码。
- [ ] 🔴 **GPIO 输出顺序 ≠ 寄存器位顺序**：`K230DwSsiIrq` 枚举与 `k230_dw_ssi_irq_status_mask[]` 映射；`update_irq` 按枚举遍历设引脚。
  - 为什么：上游常因「IRQ 位序=GPIO 序」错误而打回；此处已有显式映射，确认无误。

### 12.2 更新点
- [ ] 🔴 **所有改变 IRQ 状态的路径都调 `k230_dw_ssi_update_irq`**：push_tx、read DR、run_transfer、写 IMR/TXFTLR/RXFTLR/SSIENR/SER/DMACR、read_clear、复位、post_load、abort、idma_end。
  - 怎么验证：grep `update_irq` 调用点对照「状态变更点」清单，无遗漏。

### 12.3 读清
- [ ] 🔴 **每个读清寄存器返回 `!!active`**（0/1），而非原始掩码；清后 `irq_latched &= ~clear_mask` 再 update。
- [ ] 🟡 **`ICR` 清多个中断**（TXO|RXO|RXU|MST）；`TXEICR` 清 TXO|TXU；与 DWC 手册一致。

### 12.4 电平 vs 边沿
- [ ] 🟡 **`TXE`（TX 空）与 `RXF`（RX 满）为电平**，从 latch 派生时不要重复置；`TXO/RXO/TXU/RXU/MST/DONE/AXIE` 为边沿锁存。

---

## 13. 复位（已在 §4.3，此处补强）

- [ ] 🔴 **`enter` 完全清零 `regs[]` + FIFO reset + phase/remaining/enhanced/irq_latched/idma_completed/sleep**，再写复位值。
- [ ] 🔴 **`hold` 处理引脚**：`active_cs=-1`，所有 cs_lines raise。
- [ ] 🔴 **`exit` 重算 IRQ**（`update_irq`），因为 enter 已清 IMR 复位值。
- [ ] 🟡 **复位是幂等的**：连发两次 reset 结果一致。

---

## 14. 迁移（已在 §4.4，此处补强）

- [ ] 🔴 **派生字段不入 vmsd**：`SR`（由 FIFO/phase 算）、`ISR/RISR`（由 latch+阈值算）不在 `.fields`；只存原始状态。
- [ ] 🟡 **`enhanced` 子结构逐字段列出**（不能用 `VMSTATE_STRUCT` 不带 vmsd）；已逐字段列出 ✓。
- [ ] 🟢 **`active_cs` 用 `VMSTATE_INT32`**（-1 合法）✓。

---

## 15. 日志与可诊断性

- [ ] 🔴 **所有 guest 错误用 `qemu_log_mask(LOG_GUEST_ERROR, ...)`**，含 `DEVICE(s)->canonical_path` 与偏移/值。
- [ ] 🔴 **未实现特性用 `LOG_UNIMP`**（固定地址 IDMA、非 8-bit IDMA）。
- [ ] 🟡 **`trace-events`**：建议为关键路径（transfer start/end、IRQ raise/lower、XIP hit、IDMA start）加 trace 点；若无则确认 maintainer 不强制。
  - 怎么验证：`scripts/cleanup-trace-events.pl hw/ssi/trace-events`。

- [ ] 🟢 **无 `printf`/`fprintf`/`DPRINTF`** 调试残留。

---

## 16. qtest 覆盖

### 16.1 基础
- [ ] 🔴 **复位值测试**：复位后读所有有复位值的寄存器，断言等于宏。
- [ ] 🔴 **RW 掩码测试**：写全 1/全 0，读回应只反映可写位。
- [ ] 🔴 **坏偏移 / 非对齐 / 越界访问**：打 LOG_GUEST_ERROR（用 `g_test_expect_message` 捕获）。
- [ ] 🔴 **写已启用寄存器拒绝**：enable 后写 CTRLR0 等，读回应不变 + 捕获错误日志。

### 16.2 功能
- [ ] 🔴 **四 TMOD 各一轮**：挂一个假 SPI flash 设备（或 `ssi_create_bus` 上挂 test slave），校验 RX 数据。
- [ ] 🔴 **增强模式 Dual/Quad**：trans_type 0/1/2 × spi_frf 1/2，校验指令/地址/dummy/data 序列与 NDF。
- [ ] 🔴 **XIP 读**：使能 hi_sys XIP，读窗口地址，校验字节序与内容。
- [ ] 🔴 **IDMA RO/TO**：校验内存↔flash 数据、DONE/AXIE 中断、SSIENR 结束清零。
- [ ] 🟡 **FIFO 溢出/下溢**：TX 满 push → TXO；RX 空读 → RXU；阈值中断。
- [ ] 🟡 **多 CS**：`num_cs>1` 实例，写 SER 多比特报错。
- [ ] 🟡 **读清寄存器**：读后 latch 清零、IRQ 拉低。
- [ ] 🟡 **复位三阶段**：复位中/后状态正确。

### 16.3 注册
- [ ] 🔴 **`tests/qtest/meson.build` 注册**，`slow`/`quick` 分类与邻居一致；CI 能跑到。
- [ ] 🟡 **qtest 不依赖宿主 SPI 硬件**，纯模型内自洽。

---

## 17. SoC 集成层（`hw/riscv/k230.c` + `k230_hi_sys`）

### 17.1 创建与装配
- [ ] 🔴 **`K230SoCState` 含 `K230DwSsiState dw_ssi[3]`**（SPI + 2×QSPI），通过 `object_initialize_child` / `sysbus_realize` 装配。
- [ ] 🔴 **每实例 `num-cs`/`max-lines` 属性按 TRM 设置**：SPI 通常 max-lines=1，QSPI/flash 控制器 max-lines=4 或 8。
  - 怎么验证：读 SoC realize 里的 `qdev_prop_set_uint32`。

### 17.2 内存映射
- [ ] 🔴 **mmio 基址与 TRM 一致**：SPI0=`0x91584000`、SPI1=`0x91582000`、SPI2=`0x91583000`（与 qtest 宏一致）。
- [ ] 🔴 **XIP 窗口映射到 `0xc0000000`**（`K230_FLASH_BASE`），大小 128M。
- [ ] 🟡 **`sysbus_mmio_map` 错误检查**：返回值或 `&error_fatal`。

### 17.3 中断
- [ ] 🔴 **PLIC IRQ 号与 TRM/SDK DT 一致**：`K230_SPI0_IRQ_BASE=146`、SPI1=155、SPI2=164，每个 base + 9 个 IRQ（TXE/TXO/RXF/RXO/TXU/RXU/MST/DONE/AXIE）连到 PLIC。
  - 怎么验证：对照 SDK `k230.dtb` 的 interrupt 列表。

- [ ] 🟡 **9 个 IRQ 引脚都连**，无悬空；GPIO 顺序与控制器枚举一致。

### 17.4 flash 与 hi_sys
- [ ] 🔴 **SPI flash 设备挂载**：QSPI 实例上挂 `m25p80`（或同类），型号由 `spi_flash_model` 属性传入；CS 连线正确。
- [ ] 🔴 **`k230_dw_ssi_set_hi_sys` 把 hi_sys 注入每个 ssi 实例**，XIP 门控才能工作。
- [ ] 🟡 **hi_sys 的 XIP 使能位（`K230_SSI_CTRL_XIP_EN`）路由到对应 SSI**；`K230_SSI_CTRL_SPI0_SLEEP` 等睡眠位与 `sleep_status` 联动。

### 17.5 板级
- [ ] 🟡 **`-machine k230` 默认参数合理**：CPU 数、RAM 大小、bios 路径。
- [ ] 🟢 **`k230.rst` 文档与实际命令行一致**：示例可跑通。

---

## 18. 文档

- [ ] 🔴 **`docs/system/riscv/k230.rst` 列出 SPI/QSPI 支持情况**：标准/Dual/Quad/XIP/IDMA 哪些已建模、哪些未实现。
- [ ] 🟡 **引用 K230 TRM 版本/章节**（已在头注释 V0.3.1，文档也建议加）。
- [ ] 🟡 **`docs/system/target-riscv.rst` 的 K230 段落与机器能力描述同步**。
- [ ] 🟢 **运行示例命令可复制粘贴运行**（已检查，命令完整）。

---

## 19. 代码风格（QEMU）

- [ ] 🔴 **4 空格缩进，无 Tab**；`scripts/checkpatch.pl` 通过即保证。
- [ ] 🔴 **命名**：函数/变量 `snake_case`，宏 `UPPER_SNAKE`，类型 `K230DwSsiState`（CamelCase）。
- [ ] 🟡 **`g_autofree`/`g_autoptr`** 优先于手动 free（IDMA buffer 已用 `g_autofree`）✓。
- [ ] 🟡 **无 `//` 单行注释**，全用 `/* */`。
- [ ] 🟢 **局部变量声明在块首**（C89 风格，QEMU 仍偏好）；循环变量 `for (int i...)` 已被接受 ✓。

---

## 域级出口标准（每个域全绿才算该域过）

| 域 | 出口标准 |
|---|---|
| 进程 | DCO+checkpatch+bisect 全绿，cover letter 完整 |
| 构建 | Kconfig 门控正确，meson 注册正确，全平台不误编 |
| QOM | realize 不 exit，资源配对，三阶段 reset，vmsd 完整 |
| 寄存器 | REG32/FIELD 全覆盖，掩码/复位/RAZWI/写拒绝/坏偏移 全 |
| 内存区 | mmio+xip 双区，ops 尺寸正确 |
| SPI/CS | 总线+CS+极性+多选拒绝+max_lines 强制 |
| 标准传输 | 四 TMOD 全覆盖，DFS/SRL/batch/溢出 |
| 增强/QSPI | FRF/TRANS_TYPE/INST_L/ADDR_L/dummy/DDR 拒绝 |
| XIP | hi_sys 门控+序列+字节序+写拒绝 |
| IDMA | 使能条件+64位地址+溢出+dma返回值+结束清理 |
| 中断 | 三态+VALID_MASK+GPIO顺序+更新点全覆盖+读清 |
| 复位 | 三阶段+幂等+引脚+IRQ重算 |
| 迁移 | 派生不入vmsd+全状态+post_load |
| 日志 | GUEST_ERROR/UNIMP 分类正确+canonical_path |
| qtest | 复位/掩码/四TMOD/增强/XIP/IDMA/IRQ/溢出/多CS/读清 |
| 集成 | 三实例+属性+内存图+PLIC号+flash+hi_sys |
| 文档 | k230.rst 列能力+TRM引用 |
| 风格 | checkpatch strict 零警告 |

---

## 附录 A：Patch ↔ 审查域映射（建议拆分参考）

> 你的 ~10 个 patch 与审查域的对应关系。可按此对照每个 patch 该重点自审哪些域。

| # | Patch 主题（建议） | 主要审查域 |
|---|---|---|
| 1 | `hw/misc: Add K230 hi_sys controller` | §2 §3 §4 §5 §6 §15 |
| 2 | `hw/ssi: Add K230 DWC SSI register layout & skeleton` | §2 §3 §4 §5 §19 |
| 3 | `hw/ssi: K230 SSI standard SPI transfer (4 TMOD)` | §7 §8 §12 |
| 4 | `hw/ssi: K230 SSI enhanced Dual/Quad (QSPI) mode` | §7 §9 §12 |
| 5 | `hw/ssi: K230 SSI XIP read window` | §6 §10 §12 |
| 6 | `hw/ssi: K230 SSI indirect DMA (IDMA)` | §11 §12 |
| 7 | `hw/ssi: K230 SSI reset, migration, IRQ finalization` | §4.3 §4.4 §12 §13 §14 |
| 8 | `hw/riscv: K230 SoC integrate SSI/QSPI + flash` | §17 |
| 9 | `tests/qtest: Add K230 SSI qtest` | §16 |
| 10 | `docs/MAINTAINERS: K230 SPI/QSPI docs & maintainer` | §1.2 §18 |

> 注：实际拆分以你的 cover letter 为准；若某 patch 跨多域，按「该 patch 引入的能力」对照上表。

---

## 附录 B：自审记录表（提交前填写）

> 每条条目自审后填 `[x]` + 备注。Blocker 必须全 `[x]` 才可发series。

| 域 | 🔴 总数 | 🔴 通过 | 🟡 总数 | 🟡 通过 | 备注 |
|---|---|---|---|---|---|
| 1 进程 | | | | | |
| 2 许可证 | | | | | |
| 3 构建 | | | | | |
| 4 QOM | | | | | |
| 5 寄存器 | | | | | |
| 6 内存区 | | | | | |
| 7 SPI/CS | | | | | |
| 8 标准传输 | | | | | |
| 9 增强/QSPI | | | | | |
| 10 XIP | | | | | |
| 11 IDMA | | | | | |
| 12 中断 | | | | | |
| 13 复位 | | | | | |
| 14 迁移 | | | | | |
| 15 日志 | | | | | |
| 16 qtest | | | | | |
| 17 集成 | | | | | |
| 18 文档 | | | | | |
| 19 风格 | | | | | |

---

**完。** 本文档是裁判规则，不是审查报告。你逐条自审，Blocker 全绿即可推送。
