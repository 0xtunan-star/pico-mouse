# USB 工作原理与配置报告
# USB Working Principle and Configuration Report

## 目录 / Table of Contents

1. [USB HID 设备概述 / USB HID Device Overview](#usb-hid-device-overview)
2. [TinyUSB 栈架构 / TinyUSB Stack Architecture](#tinyusb-stack-architecture)
3. [USB 描述符详解 / USB Descriptors](#usb-descriptors)
4. [配置说明 / Configuration](#configuration)
5. [代码实现 / Code Implementation](#code-implementation)
6. [工作流程 / Working Flow](#working-flow)

---

## USB HID 设备概述 / USB HID Device Overview

### 什么是 USB HID？/ What is USB HID?

**USB HID (Human Interface Device)** 是 USB 规范中定义的一类设备，用于人机交互设备，如键盘、鼠标、游戏手柄等。

**USB HID (Human Interface Device)** is a class of USB devices defined in the USB specification for human-computer interaction devices, such as keyboards, mice, and game controllers.

### 本项目中的 USB 鼠标 / USB Mouse in This Project

本项目将 Raspberry Pi Pico RP2040 配置为一个 **USB HID 鼠标设备**，通过 USB 接口连接到电脑，模拟鼠标的移动和按键操作。

This project configures the Raspberry Pi Pico RP2040 as a **USB HID mouse device**, connecting to a computer via USB to simulate mouse movement and button operations.

### USB HID 的优势 / Advantages of USB HID

- **即插即用 / Plug and Play**: 无需安装驱动程序，操作系统自动识别
- **标准化 / Standardized**: 遵循 USB HID 规范，兼容性好
- **低延迟 / Low Latency**: 10ms 报告间隔，响应迅速
- **低功耗 / Low Power**: 适合嵌入式设备

---

## TinyUSB 栈架构 / TinyUSB Stack Architecture

### TinyUSB 简介 / TinyUSB Introduction

**TinyUSB** 是一个开源的、跨平台的 USB 设备/主机栈，专为嵌入式系统设计。它支持多种 USB 设备类，包括 HID、CDC、MSC 等。

**TinyUSB** is an open-source, cross-platform USB device/host stack designed for embedded systems. It supports multiple USB device classes, including HID, CDC, MSC, etc.

### 栈结构 / Stack Structure

```
┌─────────────────────────────────────┐
│     Application Layer               │  应用层
│     (usb_mouse.c)                   │
├─────────────────────────────────────┤
│     TinyUSB HID Class               │  HID 类驱动
│     (hid_device.c)                  │
├─────────────────────────────────────┤
│     TinyUSB Core                    │  USB 核心
│     (usbd.c, tusb.c)                │
├─────────────────────────────────────┤
│     RP2040 USB Driver               │  RP2040 USB 驱动
│     (dcd_rp2040.c)                  │
├─────────────────────────────────────┤
│     Hardware (RP2040 USB Controller)│  硬件层
└─────────────────────────────────────┘
```

### 关键组件 / Key Components

1. **USB 设备控制器驱动 (DCD)** / **Device Controller Driver (DCD)**
   - 处理底层 USB 硬件通信
   - Handles low-level USB hardware communication

2. **USB 核心栈 (USBD)** / **USB Device Core Stack (USBD)**
   - 管理 USB 协议、端点、描述符
   - Manages USB protocol, endpoints, and descriptors

3. **HID 类驱动** / **HID Class Driver**
   - 实现 HID 协议，处理 HID 报告
   - Implements HID protocol and handles HID reports

4. **应用层回调** / **Application Callbacks**
   - 设备描述符、配置描述符、HID 报告描述符
   - Device descriptors, configuration descriptors, HID report descriptors

---

## USB 描述符详解 / USB Descriptors

USB 设备通过**描述符 (Descriptors)** 向主机描述自己的功能和配置。本项目实现了以下描述符：

USB devices describe their capabilities and configuration to the host through **Descriptors**. This project implements the following descriptors:

### 1. 设备描述符 / Device Descriptor

**位置 / Location**: `usb_mouse.c:321-336`

```c
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),  // 描述符长度
    .bDescriptorType    = TUSB_DESC_DEVICE,             // 设备描述符类型
    .bcdUSB             = 0x0200,                       // USB 版本 2.0
    .bDeviceClass       = 0x00,                        // 设备类（在接口中定义）
    .bDeviceSubClass    = 0x00,                        // 设备子类
    .bDeviceProtocol    = 0x00,                        // 设备协议
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,      // 端点 0 最大包大小 (64)
    .idVendor           = 0x046d,                      // 厂商 ID (Logitech)
    .idProduct          = 0xc539,                      // 产品 ID (G304)
    .bcdDevice          = 0x0100,                      // 设备版本 1.0
    .iManufacturer      = 0x01,                        // 厂商字符串索引
    .iProduct           = 0x02,                        // 产品字符串索引
    .iSerialNumber      = 0x03,                        // 序列号字符串索引
    .bNumConfigurations = 0x01                         // 配置数量
};
```

**关键字段说明 / Key Fields**:
- `idVendor` / `idProduct`: 模拟 Logitech G304 鼠标，提高兼容性
- `bMaxPacketSize0`: 控制端点（端点 0）的最大数据包大小，64 字节
- `iManufacturer` / `iProduct` / `iSerialNumber`: 指向字符串描述符的索引

### 2. 配置描述符 / Configuration Descriptor

**位置 / Location**: `usb_mouse.c:353-359`

```c
uint8_t const desc_configuration[] = {
    // 配置描述符
    TUD_CONFIG_DESCRIPTOR(
        1,                                          // 配置编号
        1,                                          // 接口数量
        0,                                          // 字符串索引
        TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,    // 总长度
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,        // 属性（支持远程唤醒）
        100                                         // 最大电流（100mA）
    ),
    
    // HID 接口描述符
    TUD_HID_DESCRIPTOR(
        0,                                          // 接口编号
        0,                                          // 字符串索引
        HID_ITF_PROTOCOL_MOUSE,                     // HID 协议（鼠标）
        sizeof(desc_hid_report),                    // HID 报告描述符长度
        0x81,                                       // 端点地址（IN 端点 1）
        CFG_TUD_HID_EP_BUFSIZE,                    // 端点缓冲区大小（64）
        10                                          // 轮询间隔（10ms）
    )
};
```

**关键参数 / Key Parameters**:
- **轮询间隔 10ms**: 主机每 10ms 查询一次鼠标数据，实现低延迟
- **IN 端点 0x81**: 设备向主机发送数据的端点（地址 1，方向 IN）

### 3. HID 报告描述符 / HID Report Descriptor

**位置 / Location**: `usb_mouse.c:343-345`

```c
const uint8_t desc_hid_report[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE))
};
```

这个宏展开后定义了标准的鼠标 HID 报告格式：

This macro expands to define the standard mouse HID report format:

- **按钮位图 (1 字节)**: 表示鼠标按键状态
- **X 轴移动 (1 字节)**: -127 到 +127 的有符号值
- **Y 轴移动 (1 字节)**: -127 到 +127 的有符号值
- **滚轮 (1 字节)**: 垂直滚轮数据
- **水平滚轮 (1 字节)**: 水平滚轮数据（本项目未使用）

### 4. 字符串描述符 / String Descriptors

**位置 / Location**: `usb_mouse.c:367-401`

字符串描述符提供可读的设备信息：

String descriptors provide human-readable device information:

```c
static char const* string_desc_arr[] = {
    "\x09\x04",          // 0: 语言 ID (英文 0x0409)
    "Logitech",          // 1: 厂商名称
    "Logitech G304",     // 2: 产品名称
    "C539-001",          // 3: 序列号
};
```

---

## 配置说明 / Configuration

### 1. TinyUSB 配置文件 (`tusb_config.h`)

**位置 / Location**: `tusb_config.h`

这是 TinyUSB 的主要配置文件，定义了 USB 栈的行为：

This is the main configuration file for TinyUSB, defining USB stack behavior:

```c
// 微控制器类型
#define CFG_TUSB_MCU                OPT_MCU_RP2040

// 启用 USB 设备模式
#define CFG_TUD_ENABLED             1

// 控制端点（端点 0）最大包大小
#define CFG_TUD_ENDPOINT0_SIZE       64

// 禁用不需要的 USB 类
#define CFG_TUD_CDC                 0  // 禁用串口通信
#define CFG_TUD_MSC                 0  // 禁用大容量存储
#define CFG_TUD_HID                 1  // 启用 HID

// HID 端点缓冲区大小
#define CFG_TUD_HID_EPIN_BUFSIZE    64   // IN 端点（设备→主机）
#define CFG_TUD_HID_EPOUT_BUFSIZE   64   // OUT 端点（主机→设备）

// USB 端口模式配置
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
```

**配置说明 / Configuration Notes**:
- **OPT_MODE_DEVICE**: 设备模式（不是主机模式）
- **OPT_MODE_FULL_SPEED**: 全速模式（12 Mbps），RP2040 支持的最高速度
- **缓冲区大小 64 字节**: 足够存储多个 HID 报告

### 2. CMakeLists.txt 配置

**位置 / Location**: `CMakeLists.txt:55-63`

CMakeLists.txt 中通过编译定义传递配置：

CMakeLists.txt passes configuration through compile definitions:

```cmake
target_compile_definitions(usb_mouse PRIVATE
    PICO_TINYUSB_IMPL=1              # 启用 TinyUSB 实现
    CFG_TUSB_MCU=OPT_MCU_RP2040     # 微控制器类型
    CFG_TUD_CDC=0                    # 禁用 CDC
    CFG_TUD_MSC=0                    # 禁用 MSC
    CFG_TUD_HID=1                    # 启用 HID
    CFG_TUD_HID_EPIN_BUFSIZE=64      # IN 端点缓冲区
    CFG_TUD_HID_EPOUT_BUFSIZE=64     # OUT 端点缓冲区
)
```

**注意 / Note**: 这些定义会覆盖 `tusb_config.h` 中的同名定义，确保配置一致性。

---

## 代码实现 / Code Implementation

### 1. USB 初始化 / USB Initialization

**位置 / Location**: `usb_mouse.c:277-281`

```c
// 初始化 TinyUSB
tusb_init();
if (board_init_after_tusb) {
    board_init_after_tusb();
}
```

`tusb_init()` 函数会：
- 初始化 USB 硬件控制器
- 配置端点
- 准备描述符回调函数

### 2. USB 任务循环 / USB Task Loop

**位置 / Location**: `usb_mouse.c:285-289`

```c
while (true) {
    tud_task();       // USB 协议栈任务（必须定期调用）
    hid_task();       // HID 报告生成
    bootsel_task();   // BOOTSEL 按键处理
}
```

**`tud_task()` 的作用 / Role of `tud_task()`**:
- 处理 USB 协议栈的所有事件
- 响应主机的请求（描述符、配置等）
- 管理 USB 状态机
- **必须定期调用**（建议至少每 1ms 一次）

### 3. HID 报告发送 / HID Report Sending

**位置 / Location**: `usb_mouse.c:235-259`

```c
static void hid_task(void) {
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    // 每 10ms 执行一次
    if (board_millis() - start_ms < interval_ms) {
        return;
    }
    start_ms += interval_ms;

    // 检查 USB 是否就绪
    if (!tud_hid_ready()) {
        return;
    }

    // 生成鼠标移动数据
    bool moved = generate_movement();
    
    // 发送 HID 鼠标报告
    tud_hid_mouse_report(
        REPORT_ID_MOUSE,  // 报告 ID
        0,                // 按钮位图（无按键）
        mouse_x,         // X 轴移动
        mouse_y,         // Y 轴移动
        0,                // 滚轮
        0                 // 水平滚轮
    );
    
    // 重置移动值
    mouse_x = 0;
    mouse_y = 0;
}
```

**关键函数 / Key Functions**:
- `tud_hid_ready()`: 检查 HID 接口是否已配置并准备好
- `tud_hid_mouse_report()`: 发送鼠标 HID 报告到主机

### 4. USB 事件回调 / USB Event Callbacks

**位置 / Location**: `usb_mouse.c:293-318`

TinyUSB 通过回调函数通知应用层 USB 状态变化：

TinyUSB notifies the application layer of USB state changes through callback functions:

```c
// USB 挂载回调（设备已连接到主机）
void tud_mount_cb(void) {
    usb_host_mounted = true;
    usb_suspended = false;
    ws2812_restore_status_color();  // LED 显示绿色
}

// USB 卸载回调（设备已断开）
void tud_umount_cb(void) {
    usb_host_mounted = false;
    ws2812_restore_status_color();  // LED 显示蓝色
}

// USB 挂起回调（主机进入省电模式）
void tud_suspend_cb(bool remote_wakeup_en) {
    usb_suspended = true;
    ws2812_restore_status_color();  // LED 显示黄色
}

// USB 恢复回调（主机从省电模式恢复）
void tud_resume_cb(void) {
    usb_suspended = false;
    ws2812_restore_status_color();  // LED 显示绿色
}
```

### 5. 描述符回调函数 / Descriptor Callback Functions

TinyUSB 在需要时调用这些回调函数获取描述符：

TinyUSB calls these callback functions when descriptors are needed:

```c
// 设备描述符回调
const uint8_t* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

// 配置描述符回调
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return desc_configuration;
}

// HID 报告描述符回调
const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) {
    return desc_hid_report;
}

// 字符串描述符回调
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    // 返回对应的字符串描述符
}
```

---

## 工作流程 / Working Flow

### USB 枚举过程 / USB Enumeration Process

1. **设备连接 / Device Connection**
   - Pico 通过 USB 连接到主机
   - USB 硬件检测到连接，触发中断

2. **复位和地址分配 / Reset and Address Assignment**
   - 主机发送 USB 复位信号
   - 主机分配设备地址（默认地址 0）

3. **描述符请求 / Descriptor Requests**
   - 主机请求设备描述符 → `tud_descriptor_device_cb()`
   - 主机请求配置描述符 → `tud_descriptor_configuration_cb()`
   - 主机请求字符串描述符 → `tud_descriptor_string_cb()`
   - 主机请求 HID 报告描述符 → `tud_hid_descriptor_report_cb()`

4. **配置选择 / Configuration Selection**
   - 主机选择配置（配置 1）
   - 触发 `tud_mount_cb()` 回调
   - USB 设备进入配置状态

5. **HID 通信 / HID Communication**
   - 主机开始轮询 HID 报告（每 10ms）
   - 设备通过 `tud_hid_mouse_report()` 发送鼠标数据
   - 主机接收数据并移动鼠标光标

### 数据流 / Data Flow

```
┌─────────────┐
│  主机 (PC)   │
└──────┬──────┘
       │ USB 总线
       │
       ▼
┌─────────────────┐
│  RP2040 USB     │  ← USB 硬件控制器
│  Controller     │
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│  TinyUSB DCD    │  ← 设备控制器驱动
│  (dcd_rp2040.c) │
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│  TinyUSB Core   │  ← USB 核心栈
│  (usbd.c)       │
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│  HID Class      │  ← HID 类驱动
│  (hid_device.c) │
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│  Application    │  ← 应用层
│  (usb_mouse.c)  │
└─────────────────┘
```

### 时序图 / Timing Diagram

```
主机 (Host)                    RP2040 (Device)
   │                              │
   │─── USB Reset ───────────────>│
   │                              │
   │<── Get Device Descriptor ───│
   │─── Device Descriptor ──────>│
   │                              │
   │<── Get Configuration ───────│
   │─── Configuration ───────────>│
   │                              │
   │<── Set Configuration ────────│
   │                              │ tud_mount_cb()
   │                              │
   │<─── HID Report (10ms) ──────│
   │<─── HID Report (10ms) ──────│
   │<─── HID Report (10ms) ──────│
   │         ...                  │
```

---

## 总结 / Summary

### 关键要点 / Key Points

1. **TinyUSB 栈** / **TinyUSB Stack**
   - 提供完整的 USB 设备实现
   - 通过回调函数与应用层交互
   - 支持多种 USB 设备类

2. **USB 描述符** / **USB Descriptors**
   - 设备描述符：定义设备基本信息
   - 配置描述符：定义设备配置和接口
   - HID 报告描述符：定义 HID 数据格式
   - 字符串描述符：提供可读的设备信息

3. **配置层次** / **Configuration Layers**
   - `tusb_config.h`: TinyUSB 栈配置
   - `CMakeLists.txt`: 编译时配置
   - 代码中的描述符：运行时配置

4. **工作流程** / **Working Flow**
   - 定期调用 `tud_task()` 处理 USB 事件
   - 通过 `tud_hid_mouse_report()` 发送 HID 报告
   - 通过回调函数响应 USB 状态变化

### 参考资料 / References

- [USB HID Specification](https://www.usb.org/sites/default/files/documents/hid1_11.pdf)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [Raspberry Pi Pico SDK](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)

---

**报告生成日期 / Report Generated**: 2025-11-28

