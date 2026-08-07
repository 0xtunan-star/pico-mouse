# USB Mouse for Raspberry Pi Pico RP2040
# 基于 Raspberry Pi Pico RP2040 的 USB 鼠标

A USB mouse device based on Raspberry Pi Pico RP2040 that automatically moves the cursor randomly to prevent computer sleep/shutdown.

基于 Raspberry Pi Pico RP2040 的 USB 鼠标设备，自动随机移动鼠标光标以防止电脑休眠/关机。

## Features / 功能特性

- **Automatic Random Movement / 自动随机移动**: Randomly moves the mouse cursor to prevent computer from going to sleep or shutting down
- **USB HID Device / USB HID 设备**: Appears as a standard USB mouse to the computer
- **Low Power Consumption / 低功耗**: Efficient operation using RP2040 microcontroller
- **Plug and Play / 即插即用**: No additional drivers required

- **自动随机移动**: 随机移动鼠标光标，防止电脑进入休眠或关机状态
- **USB HID 设备**: 在电脑上显示为标准 USB 鼠标设备
- **低功耗**: 使用 RP2040 微控制器实现高效运行
- **即插即用**: 无需安装额外驱动程序
- **WS2812 LED 状态指示**: 通过 WS2812 LED 显示设备工作状态

## Building / 构建

This project uses CMake with Ninja build system. / 本项目使用 CMake 和 Ninja 构建系统。

### Prerequisites / 前置要求
- CMake 3.13 or later / CMake 3.13 或更高版本
- Ninja build system / Ninja 构建系统
- Raspberry Pi Pico SDK / Raspberry Pi Pico SDK
- ARM GCC toolchain / ARM GCC 工具链

### Build Steps / 构建步骤

1. **Configure the project / 配置项目**:
   ```bash
   mkdir build
   cd build
   cmake .. -G "Ninja"
   ```

2. **Build the project / 编译项目**:
   ```bash
   ninja -j10
   ```

3. **Build the project / 烧录到板子**:
先长按BOOTSEL按钮，再连接电脑
   ```bash
   ninja flash
   ```

3. **Output files / 输出文件**:
   - `usb_mouse.uf2` - Firmware file for Pico / Pico 固件文件
   - `usb_mouse.elf` - ELF binary / ELF 二进制文件
   - `usb_mouse.hex` - Intel HEX format / Intel HEX 格式

### Upload to Pico / 上传到 Pico

1. Press and hold the BOOTSEL button on your Pico / 按住 Pico 上的 BOOTSEL 按键
2. Connect the Pico to your computer via USB / 通过 USB 将 Pico 连接到电脑
3. Drag the `usb_mouse.uf2` file to the Pico drive / 将 `usb_mouse.uf2` 文件拖拽到 Pico 驱动器

## WS2812 LED / WS2812 LED 指示灯

### Pin Connection / 引脚连接
- **WS2812 Data Pin / WS2812 数据引脚**: GPIO 16
- Connect the WS2812 data line to GPIO 16 on the Pico
- 将 WS2812 的数据线连接到 Pico 的 GPIO 16

### Drive Method / 驱动方式
- **PIO (Programmable I/O) / PIO (可编程 I/O)**: Uses RP2040's PIO state machine for precise timing control
- **Official Program / 官方程序**: Uses the official `ws2812_program` from Pico SDK
- **Frequency / 频率**: 800kHz (800,000 Hz)
- **Color Format / 颜色格式**: RGB input, converted to GRB output for WS2812 protocol
- **驱动方式**: 使用 RP2040 的 PIO 状态机进行精确时序控制
- **官方程序**: 使用 Pico SDK 官方的 `ws2812_program`
- **频率**: 800kHz (800,000 Hz)
- **颜色格式**: RGB 输入，转换为 GRB 输出以符合 WS2812 协议

### Status Colors / 状态颜色
- **Amber / 琥珀色**: Booting / 启动中
- **Blue / 蓝色**: USB ready / USB 就绪
- **Green / 绿色**: Host connected, random movement enabled / 主机已连接，随机移动已启用
- **Purple / 紫色**: Activity flash / 活动闪烁
- **Yellow / 黄色**: USB suspended / USB 挂起
- **Red / 红色**: Error / 错误

## Button Control / 按键控制

### BOOTSEL Button / BOOTSEL 按键
- **Function / 功能**: Toggle random mouse movement on/off / 切换随机鼠标移动的开启/关闭
- **Usage / 使用方法**: Press the BOOTSEL button on the Pico to start/stop random movement / 按下 Pico 上的 BOOTSEL 按键来启动/停止随机移动
- **Debounce / 防抖**: 300ms guard interval to prevent accidental toggles / 300ms 防抖间隔，防止意外切换
- **LED Feedback / LED 反馈**: 
  - Purple flash / 紫色闪烁: Random movement enabled / 随机移动已启用
  - Blue/Green flash / 蓝/绿色闪烁: Random movement disabled / 随机移动已禁用

### 按键控制说明
- **功能**: 通过 BOOTSEL 按键切换随机鼠标移动功能的开启和关闭
- **使用方法**: 按下 Pico 板上的 BOOTSEL 按键即可切换状态
- **防抖处理**: 300ms 防抖间隔，避免误触
- **LED 反馈**: 
  - 紫色闪烁表示随机移动已启用
  - 蓝/绿色闪烁表示随机移动已禁用
