# openvela RK3566 BSP

openvela (基于 NuttX) 移植到 Rockchip RK3566 KICKPI K11C 开发板。

## 状态

✅ NuttX 内核启动成功，NSH Shell 可交互。

## 目录结构

```
├── nuttx/
│   ├── arch/arm64/
│   │   ├── src/rk3566/          # 芯片层：启动、串口、MMU
│   │   ├── include/rk3566/      # 芯片头文件（chip.h, irq.h）
│   │   └── Kconfig              # 需要添加 ARCH_CHIP_RK3566
│   └── boards/arm64/rk3566/
│       └── kickpi_k11c/         # 板级层：defconfig、链接脚本
├── patches/                     # NuttX 核心修改补丁
├── dts/                         # 设备树文件
└── DEVLOG.md                    # 开发日志
```

## 关键信息

- **Console**: UART2 @ 0xfe660000, 1500000 波特率
- **内存**: 2GB RAM, 起始 0x02000000
- **GIC**: GICv3 @ 0xfd400000
- **启动方式**: U-Boot `go 0x02080040`
- **异常级别**: EL2（U-Boot `go` 不降级）

## 启动方法

```bash
# U-Boot 命令行
setenv bootcmd 'mmc read 0x02080000 0x8000 0x1000; go 0x02080040'
boot
```

## EL2 适配说明

由于 U-Boot 的 `go` 命令不处理异常级别切换，NuttX 运行在 EL2。需要修改以下 NuttX 核心文件：

| 文件 | 修改内容 |
|------|----------|
| arm64_head.S | 关闭 U-Boot MMU、EL2 向量表 |
| arm64_fatal.c | EL2 SVC 异常处理 |
| arm64_vector_table.S | EL2 elr/spsr 寄存器 |
| arm64_vectors.S | EL2 esr/spsr 寄存器 |
| arm64_initialstate.c | 任务 SPSR 用 EL2H |
| arm64_schedulesigaction.c | 信号 SPSR 用 EL2H |
| arm64_syscall.c | 系统调用 SPSR 用 EL2H |
| arm64_allocateheap.c | 堆大小计算修复 |
| arm64_mmu.c | EL2 MMU 支持 |

## 编译方法

```bash
cd nuttx
export PATH="/path/to/aarch64-none-elf/bin:$PATH"
./tools/configure.sh kickpi_k11c:nsh
make -j$(nproc)
```

## 硬件参考

- DTS: `dts/my_board_schematic.dts`
- DTB: `dts/my_board.dtb`
