# RK3566 openvela/NuttX 移植开发日志

> 目标：将 openvela 移植到 RK3566 KICKPI K11C 开发板，最终实现语音点歌、视频播放、摄像头视觉问答等 AI Agent 功能。

---

## 2026-06-06：NuttX 首次启动成功

### 重大突破

**NuttX 在 RK3566 KICKPI K11C 上成功启动，NSH Shell 可交互！**

### 解决的关键问题

| 问题 | 原因 | 修复 |
|------|------|------|
| EL2→EL1 `eret` 崩溃 | U-Boot 的 MMU 开着，`eret` 在虚拟地址空间执行失败 | 在 `real_start` 最开头关闭 EL2 MMU |
| CONFIG_RAM_END 符号扩展 | `0x80000000` 在 64 位运算中被符号扩展为 `0xFFFFFFFF80000000` | 用 `((uintptr_t)0x8000) << 16` 构造，避免编译器编码问题 |
| SVC 异常在 EL2 不处理 | 异常处理器只处理 EL1，EL2 直接返回错误 | 在 `arm64_fatal.c` 的 EL2 case 中添加 SVC 处理 |
| 向量表 EL1 寄存器 | 向量表读写 `esr_el1`/`spsr_el1`，EL2 下无效 | 添加 `#elif CONFIG_ARCH_ARM64_EXCEPTION_LEVEL == 2` 分支 |
| 任务 SPSR 模式错误 | 新任务 SPSR 设为 `SPSR_MODE_EL1H`，但 CPU 在 EL2 | 改为 `SPSR_MODE_EL2H` |

### 修改的文件清单

**NuttX 核心修改（EL2 适配）：**
- `arch/arm64/src/common/arm64_head.S` — 关闭 U-Boot MMU、EL2 向量表设置
- `arch/arm64/src/common/arm64_fatal.c` — EL2 SVC 异常处理
- `arch/arm64/src/common/arm64_vector_table.S` — EL2 elr/spsr 读写
- `arch/arm64/src/common/arm64_vectors.S` — EL2 esr/spsr 读写
- `arch/arm64/src/common/arm64_initialstate.c` — 任务 SPSR 用 EL2H
- `arch/arm64/src/common/arm64_schedulesigaction.c` — 信号 SPSR 用 EL2H
- `arch/arm64/src/common/arm64_syscall.c` — 系统调用 SPSR 用 EL2H
- `arch/arm64/src/common/arm64_allocateheap.c` — 堆大小计算修复
- `arch/arm64/src/common/arm64_mmu.c` — EL2 MMU 支持（TCR、断言）
- `include/nuttx/config.h` — CONFIG_RAM_END 类型修复

**RK3566 BSP 文件：**
- `arch/arm64/src/rk3566/` — 芯片支持（9 个文件）
- `arch/arm64/include/rk3566/` — 芯片头文件（chip.h, irq.h）
- `boards/arm64/rk3566/kickpi_k11c/` — 板级支持（11 个文件）
- `arch/arm64/Kconfig` — 注册 RK3566 芯片
- `boards/Kconfig` — 注册 KICKPI K11C 板

### 启动方式

```bash
# U-Boot 命令行
setenv bootcmd 'mmc read 0x02080000 0x8000 0x1000; go 0x02080040'
boot
```

### 当前状态

- ✅ 内核启动
- ✅ 串口控制台（UART2 @ 1500000 波特率）
- ✅ NSH Shell 可交互
- ✅ 基本命令可用（ls, cd 等）
- ⚠️ 运行在 EL2（非标准，部分功能可能受限）
- ❌ 网络未就绪
- ❌ 外设驱动未开发

### 下一步

1. 配置网络（以太网或 WiFi）
2. 开发基础外设驱动（GPIO、I2C、SPI）
3. 适配 openvela 独有组件
4. 开发 AI Agent 应用

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

- **Console UART**: UART2 @ 0xfe660000，1500000 波特率（从 DTB bootargs 确认：`earlycon=uart8250,mmio32,0xfe660000`）
- **内存布局**: 2GB RAM，起始 0x02000000（从 DTS memory 节点提取）
- **GIC**: GICv3 @ 0xfd400000（RK3566 使用 GICv3，与 RK3399 的 GICv2 不同）
- **UART IP**: DW APB UART（8250 兼容），与 RK3399 使用相同 IP 核，寄存器布局一致
- **UART 时钟**: 24MHz（xin24m），由 U-Boot 配置好
- **启动方式**: U-Boot 加载 nuttx.bin 到 0x02080000，`go 0x02080000` 启动

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
