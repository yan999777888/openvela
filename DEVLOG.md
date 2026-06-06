# RK3566 openvela/NuttX 移植开发日志

> 目标：将 openvela 移植到 RK3566 KICKPI K11C 开发板，最终实现语音点歌、视频播放、摄像头视觉问答等 AI Agent 功能。

---

## 2026-06-05：基础 BSP 移植（第一阶段起步）

### 完成内容

**从零创建 RK3566 NuttX BSP，编译通过。**

#### 1. Chip 支持层 (`nuttx/arch/arm64/src/rk3566/`)

| 文件 | 说明 |
|------|------|
| `Kconfig` | 定义 RK3566 UART0-9 外设选项 |
| `Make.defs` | 声明 chip 源文件（boot.c, serial.c, lowputc.S） |
| `chip.h` | 芯片头文件 |
| `hardware/rk3566_memorymap.h` | 外设基地址（GPIO/UART/I2C/SPI/USB/GMAC 等） |
| `rk3566_boot.c` | 启动代码：MMU 初始化、PSCI、board_initialize |
| `rk3566_boot.h` | 启动头文件 |
| `rk3566_serial.c` | DW APB UART 驱动，console=UART2 @ 0xfe660000 |
| `rk3566_serial.h` | UART 地址和 IRQ 号定义 |
| `rk3566_lowputc.S` | 早期串口输出（汇编），用于 arm64_head.S 阶段调试 |

#### 2. Board 支持层 (`nuttx/boards/arm64/rk3566/kickpi_k11c/`)

| 文件 | 说明 |
|------|------|
| `Kconfig` | 板级 Kconfig 入口 |
| `configs/nsh/defconfig` | NSH 最小配置（UART2 1500000 波特率） |
| `include/board.h` | 板级头文件 |
| `scripts/dramboot.ld` | 链接脚本，加载地址 0x02080000 |
| `scripts/Make.defs` | 编译配置，引用 Toolchain.defs |
| `src/kickpi_k11c.h` | 板级头文件 |
| `src/kickpi_k11c_boardinit.c` | 板级硬件初始化 |
| `src/kickpi_k11c_appinit.c` | 应用初始化 |
| `src/Makefile` | 源文件 Makefile |
| `src/etc/init.d/rc.sysinit` | 系统初始化脚本 |
| `src/etc/init.d/rcS` | 启动脚本 |

#### 3. Include 层 (`nuttx/arch/arm64/include/rk3566/`)

| 文件 | 说明 |
|------|------|
| `chip.h` | GICv3 基地址、内存布局（2GB RAM @ 0x02000000） |
| `irq.h` | 中断号定义（NR_IRQS=256，UART IRQ 148-157） |

#### 4. 修改的系统文件

| 文件 | 修改内容 |
|------|----------|
| `nuttx/arch/arm64/Kconfig` | 添加 `ARCH_CHIP_RK3566`（选择 Cortex-A55） |
| `nuttx/boards/Kconfig` | 注册 `ARCH_BOARD_KICKPI_K11C` |

### 关键技术决策

- **Console UART**: UART2 @ 0xfe660000，1500000 波特率（从 DTB bootargs 确认）
- **内存布局**: 2GB RAM，起始 0x02000000（从 DTS memory 节点提取）
- **GIC**: GICv3 @ 0xfd400000
- **UART IP**: DW APB UART（8250 兼容）
- **启动方式**: U-Boot `go 0x02080040`

### 编译结果

```
nuttx:     ELF 64-bit LSB executable, ARM aarch64, 4.0MB
nuttx.bin: raw binary, 316KB
工具链:    arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-elf
```

### 已知问题

- `genromfs` 未安装，暂时禁用了 `CONFIG_ETC_ROMFS`
- GPIO/网络/USB 等外设驱动尚未开发

### 下一步

1. 烧录 `nuttx.bin` 到 K11C 开发板，U-Boot 引导测试
2. 验证串口控制台输出和 NSH Shell 交互
3. 开发 GMAC 以太网驱动（AI Agent 需要网络调用 LLM API）
4. 开发 GPIO/I2C/SPI 基础外设驱动

---

## 2026-06-06：NuttX EL1 启动成功

### 重大突破

**NuttX 在 RK3566 KICKPI K11C 上以 EL1 模式成功启动，NSH Shell 可交互！**

### EL2→EL1 降级方案

U-Boot 的 `go` 命令不处理异常级别切换，CPU 始终停留在 EL2。通过在 `arm64_head.S` 的 `real_start` 最开头插入降级代码解决：

```assembly
/* 清除 HCR_EL2 的关键位 */
mrs    x0, hcr_el2
bic    x0, x0, #(1 << 27)     /* TGE: 陷出通用异常 */
bic    x0, x0, #(1 << 4)      /* IMO: 陷出 IRQ */
bic    x0, x0, #(1 << 3)      /* FMO: 陷出 FIQ */
bic    x0, x0, #(1 << 5)      /* AMO: 陷出 SError */
orr    x0, x0, #(1 << 31)     /* RW=1: EL1 运行 AArch64 */
msr    hcr_el2, x0

mov    x0, #0x3c5             /* SPSR: DAIF 掩码, EL1h 模式 */
msr    spsr_el2, x0
adr    x0, 1f
msr    elr_el2, x0
eret                           /* 降到 EL1 */
1:
```

### 解决的关键问题（EL1 路径）

| 问题 | 原因 | 修复 |
|------|------|------|
| `eret` 崩溃 (ESR 0x3a000000) | HCR_EL2.TGE=1 导致 EL1 异常被陷出到 EL2 | 清除 TGE/IMO/FMO/AMO 位 |
| 中断被陷到 EL2 | HCR_EL2.IMO/FMO 未清除 | 在 eret 前清除 IMO/FMO/AMO |
| CONFIG_RAM_END 符号扩展 | 0x80000000 在 64 位运算中符号扩展 | 用 `((uintptr_t)0x8000)<<16` 构造 |
| 堆大小计算错误 | 编译器把 0x80000000 编码为 mov 立即数时符号扩展 | 运行时移位构造 |

### 启动输出

```
- Ready to Boot Primary CPU
- Boot from EL1
- Boot to C runtime for OS Initialize
nx_start: Entry
GICv3 version detect
heap_start=0x20d1000, heap_size=0x7df2f000
Registering /dev/console
Registering /dev/ttyS0
Starting high-priority kernel worker thread(s)
nsh_main pid=2
Beginning Idle Loop
nsh>
```

### 当前状态

- ✅ EL1 模式运行
- ✅ 内核启动
- ✅ 串口控制台（UART2 @ 1500000 波特率）
- ✅ NSH Shell 可交互
- ✅ GICv3 中断控制器
- ✅ 标准 EL1 代码路径（MMU、异常处理、任务调度）
- ⚠️ 串口输出偶有乱码（输出竞争）
- ❌ 网络未就绪
- ❌ 外设驱动未开发

---

## 待完成事项

### 第一阶段：平台适配

- [x] 系统启动：内核启动、串口控制台、NSH Shell
- [ ] 网络连接：GMAC 以太网驱动、TCP/IP + TLS
- [ ] openvela 独有组件适配（至少 2 项）
- [ ] 新增外设驱动（至少 1 项 NuttX 上游不存在的驱动）
- [ ] 通过 openvela 官方准入测试集
- [ ] BSP 代码规范：Kconfig、defconfig、链接脚本、移植指南文档

### 第二阶段：AI Agent 应用

- [ ] AI Agent 框架运行，配置 LLM 后端
- [ ] 自定义 Skill（Markdown 格式）
- [ ] 自定义 Tool（C 语言，硬件交互）
- [ ] 完整闭环演示（指令→推理→工具→结果）

### 扩展任务（加分项）

- [ ] 语音交互（ASR/TTS）
- [ ] MCP Server
- [ ] LVGL 聊天 UI
- [ ] 向 openvela 社区提交 PR
