# ESP32 + MAX30102 心率血氧监测系统 —— 从零开始的完整教程

> 本教程假设你是完全的小白，会手把手带你从"一行代码没有"到"完整的心率血氧监测系统"。
> 整个项目分三个阶段，每个阶段都会先讲原理，再写代码，最后验证效果。

---

## 目录

- [项目简介](#项目简介)
- [你需要准备什么](#你需要准备什么)
- [开发环境搭建](#开发环境搭建)
- [第一阶段：跑通硬件](#第一阶段跑通硬件)
- [第二阶段：信号处理](#第二阶段信号处理)
- [第三阶段：PC 上位机可视化](#第三阶段pc-上位机可视化)
- [面试准备要点](#面试准备要点)

---

## 项目简介

### 我们要做什么？

一个**便携式心率血氧检测仪**，包含三个部分：

```
┌──────────┐    I2C    ┌──────────┐    USB串口    ┌──────────┐
│ MAX30102 │ ────────→ │  ESP32   │ ────────────→ │ PC 上位机 │
│ 光学传感器│           │ 单片机   │               │ Python   │
│          │           │          │               │          │
│ 发射红光  │           │ 信号滤波  │               │ 实时波形  │
│ 发射红外光│           │ 峰值检测  │               │ CSV导出  │
│ 接收反射光│           │ 心率计算  │               │          │
│          │           │ 血氧计算  │               │          │
└──────────┘           └──────────┘               └──────────┘
```

### 涉及到的知识领域

| 领域 | 具体内容 |
|------|---------|
| 嵌入式开发 | C++、Arduino 框架、I2C 通信协议 |
| 信号处理 | 中值滤波、均值滤波、峰值检测算法 |
| 生物医学光学 | 朗伯-比尔定律、PPG 信号、血氧原理 |
| 桌面应用开发 | Python、串口通信、matplotlib 实时绘图 |

---

## 你需要准备什么

### 硬件清单

| 硬件 | 价格参考 | 说明 |
|------|---------|------|
| ESP32 开发板 | ~20 元 | 任意型号（ESP32-DevKitC、ESP32-WROOM 等） |
| MAX30102 模块 | ~10 元 | 心率血氧传感器模块（不是 MAX30100，注意区分） |
| 杜邦线 4 根 | ~1 元 | 母对母或母对公，看你的开发板引脚类型 |
| Micro-USB 数据线 | 家里应该有 | **必须是数据线**，不是只能充电的那种 |

> 全套硬件成本不到 40 元，淘宝或拼多多搜索即可。

### 软件清单

| 软件 | 用途 |
|------|------|
| VSCode | 代码编辑器 |
| PlatformIO 插件 | ESP32 开发环境（装在 VSCode 里） |
| Python 3.8+ | 运行 PC 上位机程序 |
| CP2102 或 CH340 驱动 | ESP32 USB 转串口驱动（看你的开发板用哪种芯片） |

---

## 开发环境搭建

### 第一步：安装 VSCode

1. 打开 https://code.visualstudio.com/
2. 下载并安装

### 第二步：安装 PlatformIO 插件

1. 打开 VSCode
2. 按 `Ctrl+Shift+X` 打开扩展商店
3. 搜索 **PlatformIO IDE**
4. 点击 **Install**
5. 等待安装完成（第一次可能比较慢，需要下载工具链）
6. 安装完成后，VSCode 左侧栏会出现一个蚂蚁头图标

### 第三步：安装 Python

1. 打开 https://www.python.org/downloads/
2. 下载 Python 3.8 或更高版本
3. 安装时**勾选 "Add Python to PATH"**（非常重要）
4. 安装完成后，打开命令提示符，输入 `python --version` 确认安装成功

### 第四步：安装 ESP32 USB 驱动

1. 看你的 ESP32 开发板上的 USB 转串口芯片型号
   - **CP2102**：下载 Silicon Labs 驱动
   - **CH340**：下载 WCH 驱动
2. 安装驱动后，用 USB 线连接 ESP32，在设备管理器中应该能看到 COM 口（如 COM3）

### 第五步：安装 Python 依赖（第三阶段需要）

打开命令提示符，进入项目的 `pc_app` 文件夹，运行：

```bash
cd 你的项目路径\pc_app
pip install -r requirements.txt
```

这会安装两个库：
- `pyserial`：Python 串口通信库
- `matplotlib`：Python 绑图库

---

## 第一阶段：跑通硬件

> **目标：** ESP32 通过 I2C 接口读取 MAX30102 传感器的原始数据，在串口监视器上看到数字滚动。

### 背景知识：I2C 通信协议

I2C（读作 "I-squared-C"）是一种只需要**两根线**的通信协议：

```
ESP32                    MAX30102
  │                         │
  ├── GPIO 21 (SDA) ──────→ SDA   数据线（Data）
  ├── GPIO 22 (SCL) ──────→ SCL   时钟线（Clock）
  ├── 3.3V ────────────────→ VIN   供电
  └── GND ─────────────────→ GND   接地
```

**工作方式：**
- ESP32 是"主机"（Master），MAX30102 是"从机"（Slave）
- 每个从机有一个固定地址，MAX30102 的地址是 `0x57`
- 主机通过 SCL 线提供时钟节拍，通过 SDA 线收发数据
- GPIO 21 和 GPIO 22 是 ESP32 **默认的** I2C 引脚，不需要额外配置

### 接线

按下图接好 4 根杜邦线：

```
MAX30102 模块        ESP32 开发板
─────────────        ───────────
VIN  ──────────────→ 3.3V
GND  ──────────────→ GND
SCL  ──────────────→ GPIO 22
SDA  ──────────────→ GPIO 21
```

> **注意：** VIN 接 3.3V，不要接 5V！虽然有些模块自带稳压器可以兼容 5V，但为了安全起见，统一用 3.3V。

### 项目配置文件：platformio.ini

在项目根目录创建 `platformio.ini`：

```ini
; PlatformIO 项目配置文件
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

; 串口监视器波特率（和代码里 Serial.begin 的值要一致）
monitor_speed = 115200

; MAX30102 驱动库（SparkFun 出品，稳定可靠）
lib_deps =
    sparkfun/SparkFun MAX3010x Pulse and Proximity Sensor Library@^1.1.2
```

**逐行解释：**
- `platform = espressif32` — 目标平台是乐鑫的 ESP32 系列
- `board = esp32dev` — 通用 ESP32 开发板（几乎所有 ESP32 板子都适用）
- `framework = arduino` — 用 Arduino 框架，语法比原生 ESP-IDF 简单很多
- `monitor_speed = 115200` — 串口监视器波特率，必须和代码里的 `Serial.begin(115200)` 一致
- `lib_deps` — PlatformIO 会自动下载 SparkFun 的 MAX30102 驱动库

### 主程序代码：src/main.cpp（第一阶段版本）

```cpp
#include <Arduino.h>
#include <Wire.h>               // I2C 通信库（ESP32 内置）
#include "MAX30105.h"           // SparkFun MAX30102 驱动

MAX30105 sensor;  // 创建传感器对象

void setup() {
    // 1. 初始化串口
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { delay(10); }
    Serial.println("=== ESP32 MAX30102 心率血氧监测 ===");

    // 2. 通过 I2C 连接传感器
    if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("错误：未检测到 MAX30102！请检查接线。");
        while (true) { delay(1000); }
    }
    Serial.println("传感器连接成功！");

    // 3. 配置传感器
    byte ledBrightness = 60;  // LED 亮度 0~255
    byte sampleAverage = 4;   // 采样平均次数
    byte ledMode = 2;         // 2 = 红光+红外光（血氧需要双波长）
    int sampleRate = 100;     // 每秒采样 100 次
    int pulseWidth = 411;     // 脉冲宽度 411μs（18bit 分辨率）
    int adcRange = 4096;      // ADC 量程

    sensor.setup(ledBrightness, sampleAverage, ledMode,
                 sampleRate, pulseWidth, adcRange);

    Serial.println("请将手指轻放在传感器上...");
    Serial.println("红光(Red)\t红外光(IR)");
}

void loop() {
    uint32_t redValue = sensor.getRed();  // 红光原始值
    uint32_t irValue  = sensor.getIR();   // 红外光原始值

    if (irValue < 50000) {
        Serial.println("未检测到手指");
    } else {
        Serial.print(redValue);
        Serial.print("\t");
        Serial.println(irValue);
    }
    delay(20);
}
```

### 代码逻辑图解

```
程序启动
  │
  ├─ setup()（只运行一次）
  │   ├─ 打开串口，波特率 115200
  │   ├─ 通过 I2C 查找 MAX30102（地址 0x57）
  │   │   ├─ 找到 → 继续
  │   │   └─ 没找到 → 报错，死循环（检查接线）
  │   └─ 配置传感器参数（LED亮度、采样率等）
  │
  └─ loop()（无限循环，每 20ms 一次）
      ├─ 读取红光值 getRed()
      ├─ 读取红外光值 getIR()
      ├─ 如果 IR < 50000 → 提示"未检测到手指"
      └─ 否则 → 打印 Red 和 IR 的原始值
```

### 传感器参数详解

| 参数 | 设定值 | 含义 |
|------|--------|------|
| ledBrightness | 60 | LED 发光亮度（0~255），越大信号越强，功耗越大 |
| sampleAverage | 4 | 硬件内部对 4 次采样取平均再输出，初步降噪 |
| ledMode | 2 | 同时使用红光和红外光（1=只有红光，不能测血氧） |
| sampleRate | 100 | 每秒 100 个采样点（心率 1~3Hz，100Hz 远超奈奎斯特频率） |
| pulseWidth | 411 | LED 单次点亮 411 微秒，对应 18bit ADC 分辨率 |
| adcRange | 4096 | ADC 量程，值越大能覆盖的光强范围越广 |

### 编译和上传

1. 用 VSCode 打开项目文件夹
2. 用 USB 线连接 ESP32
3. 点击 VSCode **底部状态栏**的 **→ 箭头按钮**（Upload），或按 `Ctrl+Alt+U`
4. PlatformIO 会自动：下载工具链 → 下载库 → 编译 → 烧录
5. 烧录完成后，点击底部的 **插头图标**（Serial Monitor），或按 `Ctrl+Alt+S`

### 预期结果

```
=== ESP32 MAX30102 心率血氧监测 ===
传感器连接成功！
请将手指轻放在传感器上...
红光(Red)    红外光(IR)
未检测到手指
未检测到手指
102345       115678       ← 手指放上去后看到两列数字
102298       115623
102401       115701
...
```

当你用手指轻按传感器时，数字会随心跳节律微微波动 —— 这就是 **PPG 信号**（光电容积脉搏波）的原始形态。

### 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| "未检测到 MAX30102" | 接线错误 | 检查 4 根线是否接对，VIN 是否接 3.3V |
| 编译报错找不到库 | 网络问题 | 确认网络通畅，PlatformIO 需要联网下载库 |
| 串口监视器乱码 | 波特率不对 | 串口监视器右下角选 115200 |
| 数据全是 0 | 传感器没初始化 | 按 ESP32 的 RST 按钮重启 |
| 数值剧烈跳动 | 手指不稳 | 轻放手指，不要用力按，保持不动 |

### 第一阶段小结

到这里，你已经完成了：
- ✅ 硬件接线（I2C 四线连接）
- ✅ 传感器驱动（通过 SparkFun 库读取数据）
- ✅ 串口输出（在电脑上看到实时原始数据）

接下来进入最核心的第二阶段：**信号处理**。

---

## 第二阶段：信号处理

> **目标：** 自己实现滤波算法、峰值检测算法和血氧计算公式，从原始数据中提取出心率（BPM）和血氧饱和度（SpO2%）。
>
> 这是整个项目中最能体现"生物医学工程"专业性的部分。

### 整体架构

第二阶段新增三个模块，每个都是一个独立的 C++ 库，放在 `lib/` 目录下：

```
项目结构：
├── lib/
│   ├── Filter/          ← 模块一：数字滤波
│   │   ├── Filter.h
│   │   └── Filter.cpp
│   ├── HeartRate/       ← 模块二：心率检测
│   │   ├── HeartRate.h
│   │   └── HeartRate.cpp
│   └── SpO2/            ← 模块三：血氧计算
│       ├── SpO2.h
│       └── SpO2.cpp
└── src/
    └── main.cpp          ← 主程序（串联所有模块）
```

数据处理流水线：

```
传感器原始数据（有噪声）
    │
    ▼
┌───────────────────┐
│  中值滤波（去毛刺） │  ← 第一级滤波
└─────────┬─────────┘
          ▼
┌───────────────────┐
│  均值滤波（平滑）   │  ← 第二级滤波
└─────────┬─────────┘
          ▼
┌───────────────────┐
│  峰值检测          │  ← 找到每一次心跳
│  R-R间期 → BPM    │
└─────────┬─────────┘
          ▼
┌───────────────────┐
│  AC/DC 比值计算    │  ← 朗伯-比尔定律
│  R值 → SpO2%      │
└───────────────────┘
```

---

### 模块一：数字滤波器（Filter）

#### 为什么需要滤波？

传感器采集到的原始信号不是完美的波形，而是叠加了各种噪声：

```
理想信号：          实际信号（有噪声）：
   ╱╲                  ╱╲
  ╱  ╲               ╱╱╲╲╱    ← 手指抖动、环境光
 ╱    ╲             ╱╱   ╲╲
╱      ╲╱          ╱       ╲╱╲
```

滤波器的作用就是把实际信号还原得尽可能接近理想信号。

#### 均值滤波（Moving Average Filter）

**原理：** 取最近 N 个采样值的平均值作为输出。

```
窗口大小 = 4

输入序列:  100, 104, 98, 102, 200(噪声), 99, 101, 103
                                  ↓
窗口滑动:  [100,104,98,102]  → 输出 101（平均值）
           [104,98,102,200]  → 输出 126（噪声被削弱了）
           [98,102,200,99]   → 输出 125
           [102,200,99,101]  → 输出 125
           [200,99,101,103]  → 输出 126（噪声继续衰减）
```

噪声值 200 原本会造成一个尖锐的毛刺，经过均值滤波后被大幅平滑。

**实现要点：**
- 使用**环形缓冲区**存储最近 N 个值，避免数组搬移
- 维护一个 `sum` 变量，每次新值来时加新减旧，不需要重新遍历求和

#### 中值滤波（Median Filter）

**原理：** 取最近 N 个采样值的**中位数**作为输出。

```
窗口大小 = 4

缓冲区:  [100, 200, 98, 102]
排序后:  [98, 100, 102, 200]
中位数:  (100 + 102) / 2 = 101

→ 异常值 200 完全不影响输出！
```

中值滤波对**突发的异常值（毛刺）**特别有效，因为排序后异常值会被推到两端。

#### 为什么两级串联？

```
原始信号 → [中值滤波] → [均值滤波] → 干净信号

第一级中值滤波：消灭突发的毛刺（硬件干扰、接触不良）
第二级均值滤波：平滑残余的高频噪声（手指微颤、环境光波动）
```

#### 代码详解：lib/Filter/Filter.h

```cpp
#define FILTER_WINDOW_SIZE 4  // 窗口大小，值越大越平滑但延迟也越大

class MeanFilter {
public:
    MeanFilter();
    uint32_t update(uint32_t newValue);  // 输入新值，返回滤波后的值
    void reset();

private:
    uint32_t _buffer[FILTER_WINDOW_SIZE];  // 环形缓冲区
    uint8_t  _index;    // 当前写入位置
    uint8_t  _count;    // 已有数据个数
    uint32_t _sum;      // 缓冲区总和（避免每次重新求和）
};

class MedianFilter {
public:
    MedianFilter();
    uint32_t update(uint32_t newValue);
    void reset();

private:
    uint32_t _buffer[FILTER_WINDOW_SIZE];
    uint8_t  _index;
    uint8_t  _count;
};
```

#### 代码详解：lib/Filter/Filter.cpp

**均值滤波核心逻辑：**

```cpp
uint32_t MeanFilter::update(uint32_t newValue) {
    // 缓冲区满了 → 减去即将被覆盖的旧值
    if (_count >= FILTER_WINDOW_SIZE) {
        _sum -= _buffer[_index];
    } else {
        _count++;
    }

    // 写入新值
    _buffer[_index] = newValue;
    _sum += newValue;

    // 环形索引：到末尾后绕回开头
    _index = (_index + 1) % FILTER_WINDOW_SIZE;

    return _sum / _count;  // 返回平均值
}
```

**中值滤波核心逻辑：**

```cpp
uint32_t MedianFilter::update(uint32_t newValue) {
    // 写入环形缓冲区
    _buffer[_index] = newValue;
    _index = (_index + 1) % FILTER_WINDOW_SIZE;
    if (_count < FILTER_WINDOW_SIZE) _count++;

    // 复制并排序（不能直接排原缓冲区，会破坏写入顺序）
    uint32_t sorted[FILTER_WINDOW_SIZE];
    for (uint8_t i = 0; i < _count; i++) sorted[i] = _buffer[i];

    // 插入排序（数据量极小，插入排序最合适）
    for (uint8_t i = 1; i < _count; i++) {
        uint32_t key = sorted[i];
        int8_t j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // 取中位数
    if (_count % 2 == 0)
        return (sorted[_count/2 - 1] + sorted[_count/2]) / 2;
    else
        return sorted[_count / 2];
}
```

---

### 模块二：心率检测（HeartRate）

#### PPG 信号与心跳的关系

每次心脏跳动时，血液涌入手指的毛细血管，导致血管体积增大，光的吸收量发生变化。这就产生了 **PPG（光电容积脉搏波）** 信号：

```
信号值
  │    ╱╲        ╱╲        ╱╲       ← 每个山峰 = 一次心跳
  │   ╱  ╲      ╱  ╲      ╱  ╲
  │  ╱    ╲    ╱    ╲    ╱    ╲
  │ ╱      ╲  ╱      ╲  ╱      ╲
  │╱        ╲╱        ╲╱        ╲
  └──────────────────────────────→ 时间
       ↑           ↑
      峰1         峰2
       |← R-R间期 →|

心率(BPM) = 60000ms / R-R间期(ms)
例如：R-R间期 = 800ms → BPM = 60000/800 = 75 次/分钟
```

#### 峰值检测算法

我们使用**动态阈值 + 斜率变化**的方法来检测峰值：

```
信号值
  │      ╱╲
  │     ╱  ╲          ← 信号上升到峰值后开始下降
  │    ╱    ╲            （从"上升"变"下降"= 拐点）
  │   ╱      ╲
阈值├──╱────────╲──    ← 动态阈值 = (近期最大值 + 近期最小值) / 2
  │ ╱          ╲
  │╱            ╲╱
  └─────────────────→ 时间

检测规则：
  1. 信号从"上升"变为"下降"（拐点出现）
  2. 信号当前在阈值之上（排除低谷处的噪声波动）
  3. 距上次峰值 > 200ms（排除高频噪声的误触发）
  → 满足以上三个条件 = 确认一次心跳！
```

**为什么用动态阈值？**

不同人、不同按压力度，PPG 信号的幅度差异很大。固定阈值无法适应所有情况。动态阈值会自动追踪信号的最大值和最小值，取中间值作为判断标准。

#### 代码详解：lib/HeartRate/HeartRate.h

```cpp
#define RR_BUFFER_SIZE 4        // 用最近 4 个 R-R 间期求平均心率
#define BPM_MIN 40              // 有效心率下限
#define BPM_MAX 200             // 有效心率上限
#define PEAK_MIN_INTERVAL_MS 200 // 两次心跳最小间隔（防误检）

class HeartRateDetector {
public:
    bool update(uint32_t irValue);      // 输入滤波后的 IR 值，返回是否检测到峰值
    int getBPM();                        // 获取当前心率
    unsigned long getLastRRInterval();   // 获取最近的 R-R 间期
    void reset();

private:
    // 峰值检测状态
    uint32_t _prevValue;           // 上一个值（判断上升/下降）
    bool     _rising;              // 当前是否上升中
    unsigned long _lastPeakTime;   // 上一个峰值的时间戳

    // 动态阈值
    uint32_t _signalMax, _signalMin;  // 信号的动态最大最小值
    uint32_t _threshold;              // 阈值 = (max+min)/2
    bool     _aboveThreshold;         // 当前是否在阈值之上

    // R-R 间期缓冲区
    unsigned long _rrIntervals[RR_BUFFER_SIZE];
    uint8_t _rrIndex, _rrCount;
};
```

#### 代码详解：lib/HeartRate/HeartRate.cpp

**动态阈值更新：**

```cpp
void HeartRateDetector::updateThreshold(uint32_t value) {
    if (value > _signalMax) _signalMax = value;
    if (value < _signalMin) _signalMin = value;

    _threshold = (_signalMax + _signalMin) / 2;

    // 缓慢衰减，让阈值能适应信号幅度的变化
    _signalMax = _signalMax - (_signalMax >> 8);  // 约 ×0.996
    _signalMin = _signalMin + (_signalMin >> 8);  // 缓慢向中间收缩
}
```

**峰值检测核心：**

```cpp
bool HeartRateDetector::update(uint32_t irValue) {
    bool peakDetected = false;
    unsigned long now = millis();

    updateThreshold(irValue);

    bool currentlyRising = (irValue > _prevValue);

    // 核心判断：上升→下降的拐点 + 在阈值之上 + 间隔足够
    if (_rising && !currentlyRising && _aboveThreshold) {
        unsigned long interval = now - _lastPeakTime;
        if (interval > PEAK_MIN_INTERVAL_MS && _lastPeakTime > 0) {
            int bpm = 60000 / interval;
            if (bpm >= BPM_MIN && bpm <= BPM_MAX) {
                // 有效心跳！记录 R-R 间期
                _rrIntervals[_rrIndex] = interval;
                _rrIndex = (_rrIndex + 1) % RR_BUFFER_SIZE;
                if (_rrCount < RR_BUFFER_SIZE) _rrCount++;
                peakDetected = true;
            }
        }
        _lastPeakTime = now;
    }

    _aboveThreshold = (irValue > _threshold);
    _rising = currentlyRising;
    _prevValue = irValue;
    return peakDetected;
}
```

**心率计算：**

```cpp
int HeartRateDetector::getBPM() {
    if (_rrCount < 2) return 0;  // 数据不足

    unsigned long sum = 0;
    for (uint8_t i = 0; i < _rrCount; i++) sum += _rrIntervals[i];

    unsigned long avgInterval = sum / _rrCount;
    return 60000 / avgInterval;  // 毫秒转换为 BPM
}
```

---

### 模块三：血氧饱和度计算（SpO2）

#### 背景知识：朗伯-比尔定律（Beer-Lambert Law）

这是整个血氧测量的物理学基础。

**基本原理：** 光穿过物质时会被吸收，吸收量取决于物质的浓度和光的波长。

```
手指横截面：

  LED 发光 → ─── 皮肤+血管+组织 ─── → 光电探测器
  红光 660nm     │                │
  红外光 880nm   │  血液在此流过   │
                 │                │
```

**关键发现：**

含氧血红蛋白（HbO₂）和脱氧血红蛋白（Hb）对不同波长光的吸收率不同：

```
吸收率
  │
  │  Hb（脱氧）
  │  ╲
  │   ╲         ╱ HbO₂（含氧）
  │    ╲       ╱
  │     ╲     ╱
  │      ╲   ╱
  │       ╲ ╱
  │        ╳      ← 大约在 800nm 处两条线交叉
  │       ╱ ╲
  │      ╱   ╲
  │     ╱     ╲
  └────┼───────┼──────→ 波长
     660nm   880nm
     红光     红外光

结论：
  - 在 660nm（红光）：Hb 吸收多，HbO₂ 吸收少
  - 在 880nm（红外光）：HbO₂ 吸收多，Hb 吸收少
```

所以：
- **血氧高** → 红光透过多（Red 值大），红外光透过少
- **血氧低** → 红光透过少（Red 值小），红外光透过多

#### 计算公式推导

**第一步：分解信号的 AC 和 DC 分量**

```
信号值
  │
  │   ╱╲
  │  ╱  ╲      ← AC：随脉搏波动的部分（交流分量）
  │ ╱    ╲        AC = 峰值 - 谷值
  │╱      ╲╱
  ├────────────  ← DC：信号的平均水平（直流分量）
  │                DC = 平均值
  └──────────→ 时间
```

- **AC 分量** 反映的是脉搏跳动引起的光吸收变化（这才是我们要的信号）
- **DC 分量** 反映的是皮肤、骨骼等不随脉搏变化的部分（背景值）

**第二步：计算 R 值**

```
         AC_red / DC_red
R  =  ────────────────────
         AC_ir  / DC_ir

其中 AC/DC 比值去除了信号幅度差异的影响，只保留血液吸收的特征。
```

**第三步：R 值转换为 SpO2**

```
SpO2 = 110 - 25 × R    （经验线性公式）

这个公式来自大量临床数据的统计拟合。

R 值范围与 SpO2 的对应关系：
  R ≈ 0.4  →  SpO2 ≈ 100%  （完全充氧）
  R ≈ 1.0  →  SpO2 ≈ 85%   （低氧）
  R ≈ 2.0  →  SpO2 ≈ 60%   （危险）
```

#### 代码详解：lib/SpO2/SpO2.h

```cpp
class SpO2Calculator {
public:
    void update(uint32_t redValue, uint32_t irValue);  // 每次采样调用
    void onBeatDetected();  // 心率模块检测到心跳时调用（一个周期结束）
    int getSpO2();          // 获取血氧百分比
    float getRValue();      // 获取 R 值（调试用）
    void reset();

private:
    // 在一个心跳周期内追踪最大值、最小值和总和
    uint32_t _redMax, _redMin;    // 红光的峰和谷
    uint32_t _irMax, _irMin;      // 红外光的峰和谷
    uint32_t _redSum, _irSum;     // 累加值（求 DC 均值用）
    uint32_t _sampleCount;        // 采样点数
    // ...
};
```

#### 代码详解：lib/SpO2/SpO2.cpp

**核心计算逻辑（在 onBeatDetected 中）：**

```cpp
void SpO2Calculator::onBeatDetected() {
    if (_sampleCount < 10) { resetCycle(); return; }  // 数据太少，不可靠

    // 计算 DC 分量（平均值）
    float dcRed = (float)_redSum / _sampleCount;
    float dcIr  = (float)_irSum / _sampleCount;

    // 计算 AC 分量（峰值 - 谷值）
    float acRed = (float)(_redMax - _redMin);
    float acIr  = (float)(_irMax - _irMin);

    // 核心公式：R = (AC_red/DC_red) / (AC_ir/DC_ir)
    float ratioRed = acRed / dcRed;
    float ratioIr  = acIr / dcIr;
    _rValue = ratioRed / ratioIr;

    // 转换为 SpO2 百分比
    float spo2 = 110.0f - 25.0f * _rValue;
    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 0.0f) spo2 = 0.0f;

    _spo2 = (int)spo2;
    resetCycle();  // 重置，准备追踪下一个心跳周期
}
```

**工作流程：**

```
时间线：
  ──┤心跳1├──────────┤心跳2├──────────┤心跳3├──
     ↑                ↑                ↑
  onBeatDetected   onBeatDetected   onBeatDetected
     │                │                │
     │  在这两次心跳之间  │                │
     │  持续调用 update()│                │
     │  追踪 max/min/sum │                │
     │                │                │
     └── 结算一次 SpO2 ─┘── 结算一次 ──→ ...
```

---

### 主程序串联：src/main.cpp

更新后的 `main.cpp` 把三个模块串联成完整的处理流水线：

```cpp
#include "Filter.h"
#include "HeartRate.h"
#include "SpO2.h"

// 创建对象
MedianFilter medianRed, medianIR;   // 第一级滤波
MeanFilter   meanRed, meanIR;       // 第二级滤波
HeartRateDetector hrDetector;        // 心率检测
SpO2Calculator    spo2Calc;          // 血氧计算

void loop() {
    // 1. 读取原始数据
    uint32_t rawRed = sensor.getRed();
    uint32_t rawIR  = sensor.getIR();

    // 2. 两级滤波
    uint32_t filteredRed = meanRed.update(medianRed.update(rawRed));
    uint32_t filteredIR  = meanIR.update(medianIR.update(rawIR));

    // 3. 心率检测（用 IR 信号，因为 IR 的信噪比通常更好）
    bool beatDetected = hrDetector.update(filteredIR);

    // 4. 血氧：每个采样点都喂给计算器追踪 AC/DC
    spo2Calc.update(filteredRed, filteredIR);
    if (beatDetected) {
        spo2Calc.onBeatDetected();  // 一个心跳周期结束，结算 SpO2
    }

    // 5. 读取结果
    int bpm  = hrDetector.getBPM();    // 心率
    int spo2 = spo2Calc.getSpO2();     // 血氧
}
```

### 预期串口输出（人类模式）

```
========================================
  ESP32 + MAX30102 心率血氧监测系统
========================================

[OK] 传感器连接成功
[OK] 传感器配置完成

请将手指轻放在传感器上，保持不动...

----------------------------------------
  原始值   Red: 102345  IR: 115678
  滤波后   Red: 102301  IR: 115623
  心率:    72 BPM  (R-R间期: 833 ms)
  血氧:    97 %  (R值: 0.520)
  累计心跳: 15 次
```

### 调参指南

如果实测效果不理想，可以调整以下参数：

| 参数 | 文件 | 默认值 | 调整方向 |
|------|------|--------|---------|
| `FILTER_WINDOW_SIZE` | Filter.h | 4 | 增大→更平滑但延迟增大 |
| `PEAK_MIN_INTERVAL_MS` | HeartRate.h | 200 | 漏检→减小；误检→增大 |
| `RR_BUFFER_SIZE` | HeartRate.h | 4 | 增大→心率显示更稳定 |
| `SPO2_AVG_COUNT` | SpO2.cpp | 4 | 增大→血氧显示更稳定 |
| `irValue < 50000` | main.cpp | 50000 | 根据实际传感器调整 |
| `ledBrightness` | main.cpp | 60 | 信号弱→增大（最大 255） |

### 第二阶段小结

到这里，你已经完成了：
- ✅ 均值滤波器和中值滤波器（两级串联去噪）
- ✅ 基于动态阈值的峰值检测算法
- ✅ 基于朗伯-比尔定律的血氧饱和度计算
- ✅ 完整的信号处理流水线

这些全部是**自己手写的算法**，不依赖任何现成的信号处理库。这正是面试时最能体现你能力的部分。

---

## 第三阶段：PC 上位机可视化

> **目标：** 用 Python 编写一个桌面程序，通过串口接收 ESP32 的数据，实时绘制 PPG 波形，显示心率和血氧，并支持 CSV 数据导出。

### 整体架构

```
ESP32 (数据模式)                     Python PC 上位机
┌────────────┐     USB 串口     ┌──────────────────────┐
│            │ ────────────────→│ 串口读取线程          │
│ 50Hz 输出   │     DATA,...    │   ↓                   │
│ DATA,t,r,  │                 │ 数据解析              │
│ ir,bpm,    │                 │   ↓                   │
│ spo2,beat  │                 │ 滑动窗口缓冲区        │
│            │ ←──────────────│   ↓                   │
│ (收到 'D'   │    发送 'D'     │ matplotlib 实时绑图   │
│  切换模式)  │                 │   + BPM/SpO2 显示     │
└────────────┘                 │   + CSV 自动导出      │
                               └──────────────────────┘
```

### 背景知识：串口通信

串口（Serial Port）是 ESP32 和电脑之间通信的桥梁：

- ESP32 通过 USB 线连接电脑后，会虚拟出一个 COM 口（Windows）或 `/dev/ttyUSB0`（Linux）
- 双方约定相同的**波特率**（115200），就可以互相收发文本数据
- ESP32 用 `Serial.println()` 发送数据，Python 用 `pyserial` 库读取

### ESP32 固件更新：双模式输出

为了让 Python 能高效解析数据，我们给 ESP32 添加了**双模式输出**：

**人类模式（默认）：** 每秒打印一次中文可读摘要 → 适合串口监视器直接查看

**数据模式（发送 'D' 激活）：** 50Hz 输出 CSV 格式 → 适合 Python 解析

```
数据模式格式：
DATA,时间戳ms,滤波红光,滤波红外,心率BPM,血氧%,心跳标志(0或1)

示例：
DATA,12345,102301,115623,72,97,0
DATA,12365,102298,115610,72,97,0
DATA,12385,102350,115680,72,97,1  ← beat=1 表示这一刻检测到心跳
```

切换方式：
- Python 通过串口发送字符 `D` → ESP32 切换到数据模式
- 发送字符 `H` → 切回人类模式

### 安装 Python 依赖

打开命令提示符：

```bash
cd 你的项目路径\pc_app
pip install -r requirements.txt
```

`requirements.txt` 内容：

```
pyserial>=3.5      # 串口通信
matplotlib>=3.5    # 图表绑制
```

### Python 代码详解：pc_app/monitor.py

#### 整体结构

```python
# 程序分为 4 个部分：

# 1. 串口连接
#    - 自动检测 COM 口，或从命令行参数指定
#    - 发送 'D' 切换 ESP32 到数据模式

# 2. 后台数据读取（独立线程）
#    - 持续读取串口数据
#    - 解析 "DATA,..." 格式的行
#    - 存入滑动窗口缓冲区

# 3. 实时图表（matplotlib 动画）
#    - 上半部分：红光 PPG 波形
#    - 下半部分：红外光 PPG 波形
#    - 标题栏显示心率和血氧
#    - 右上角心跳时闪烁 ♥

# 4. 数据导出
#    - 关闭窗口时自动保存 CSV 文件
```

#### 关键代码段讲解

**自动检测串口：**

```python
import serial.tools.list_ports

ports = list(serial.tools.list_ports.comports())
# ports 是电脑上所有串口设备的列表
# 如果只有一个（通常就是 ESP32），直接使用
# 如果有多个，让用户选择
```

**后台串口读取线程：**

```python
def serial_reader(ser):
    """在独立线程中持续读取串口"""
    while running:
        line = ser.readline().decode("utf-8").strip()
        if line.startswith("DATA,"):
            parts = line[5:].split(",")
            # parts = ["12345", "102301", "115623", "72", "97", "0"]
            t_ms, red, ir, bpm, spo2, beat = parts

            # 存入 deque（双端队列，固定长度，自动丢弃旧数据）
            time_data.append(int(t_ms) / 1000.0)
            red_data.append(int(red))
            ir_data.append(int(ir))
```

**为什么用线程？**

串口读取是**阻塞操作**（`readline()` 会等待数据到来）。如果在主线程中读取串口，图表就无法刷新。所以我们把串口读取放在独立的后台线程中，主线程专注于图表绑制。

```
主线程：             后台线程：
matplotlib          串口读取
  │                   │
  ├─ 绘图帧1          ├─ 读取 DATA 行
  ├─ 绘图帧2          ├─ 解析并存入缓冲区
  ├─ 绘图帧3          ├─ 读取 DATA 行
  ├─ ...              ├─ ...
  │                   │
  └─ 两个线程共享 deque 数据
```

**实时波形绑制（FuncAnimation）：**

```python
def update(frame):
    """每 50ms 被 matplotlib 调用一次"""
    t = list(time_data)    # 从 deque 取出时间轴
    r = list(red_data)     # 红光数据
    ir = list(ir_data)     # 红外光数据

    line_red.set_data(t, r)   # 更新红光曲线
    line_ir.set_data(t, ir)   # 更新红外光曲线

    # 自动缩放坐标轴
    ax_red.set_xlim(t[0], t[-1])
    ax_ir.set_xlim(t[0], t[-1])

    # 更新标题显示心率和血氧
    fig.suptitle(f"心率: {bpm} BPM    血氧: {spo2} %")

# 以 50ms 间隔刷新（20 FPS）
ani = animation.FuncAnimation(fig, update, interval=50)
plt.show()
```

**CSV 自动导出：**

```python
def save_csv():
    filename = f"ppg_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp_ms", "red", "ir", "bpm", "spo2", "beat"])
        writer.writerows(all_records)

# 在程序退出时自动调用
```

导出的 CSV 文件可以用 Excel 打开，也可以用 Python 做进一步分析。

### 运行步骤

1. 确保 ESP32 已经烧录了最新版固件（包含数据模式支持）
2. **关闭 PlatformIO 的串口监视器**（不能两个程序同时占用一个串口）
3. 打开命令提示符，运行：

```bash
cd 你的项目路径\pc_app
python monitor.py
```

或者指定串口：

```bash
python monitor.py COM3
```

4. 程序会自动连接 ESP32 并弹出图表窗口
5. 将手指放在传感器上，观察实时 PPG 波形
6. 关闭窗口时自动保存 CSV 文件

### 预期界面效果

```
┌─────────────────────────────────────────────┐
│   心率: 72 BPM    血氧: 97%            ♥    │
├─────────────────────────────────────────────┤
│                                             │
│  红光 PPG 波形 (660nm)                      │
│     ╱╲    ╱╲    ╱╲    ╱╲    ╱╲             │
│    ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲           │
│   ╱    ╲╱    ╲╱    ╲╱    ╲╱    ╲╱          │
│                                             │
├─────────────────────────────────────────────┤
│                                             │
│  红外光 PPG 波形 (880nm)                    │
│     ╱╲    ╱╲    ╱╲    ╱╲    ╱╲             │
│    ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲           │
│   ╱    ╲╱    ╲╱    ╲╱    ╲╱    ╲╱          │
│                                             │
└─────────────────────────────────────────────┘
```

### 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| "没有检测到任何串口设备" | USB 没接好或驱动没装 | 检查 USB 线和驱动 |
| "无法打开串口" | 串口被其他程序占用 | 关闭 PlatformIO 串口监视器 |
| 波形不动 | ESP32 没切到数据模式 | 检查 ESP32 是否正常运行 |
| 中文标题显示为方框 | 系统缺少中文字体 | 安装 SimHei 字体 |
| 波形卡顿 | 电脑性能不足 | 减小 `PLOT_WINDOW` 值 |

### 第三阶段小结

到这里，你已经完成了：
- ✅ ESP32 双模式串口输出（人类可读 / 机器可解析）
- ✅ Python 串口通信与数据解析
- ✅ matplotlib 实时 PPG 波形绑制
- ✅ 心率和血氧的实时显示
- ✅ CSV 数据自动导出

---

## 面试准备要点

### 你需要能清晰讲述的技术知识

**1. 光学原理（生物医学核心）**

> "MAX30102 交替发射 660nm 红光和 880nm 红外光，利用含氧血红蛋白和脱氧血红蛋白对这两种波长光吸收率不同的特性，通过 Beer-Lambert 定律计算血氧饱和度。"

**2. 信号处理流程**

> "原始 PPG 信号经过中值滤波去除毛刺，再经均值滤波平滑高频噪声。然后用动态阈值法检测脉搏峰值，计算 R-R 间期得到心率。同时在每个心跳周期内追踪红光和红外光的 AC/DC 分量，计算 R 值并转换为 SpO2。"

**3. 系统设计思路**

> "整个系统分为三层：传感器层（MAX30102 通过 I2C 采集数据）、处理层（ESP32 上运行滤波和算法）、展示层（PC 端实时可视化和数据导出）。三个信号处理模块（Filter、HeartRate、SpO2）被设计为独立的库，便于测试和复用。"

**4. 遇到的挑战和解决方案**

| 挑战 | 你的解决方案 |
|------|------------|
| 基线漂移 | 动态阈值自动追踪信号的最大最小值 |
| 手指抖动噪声 | 中值滤波 + 均值滤波两级串联 |
| 噪声误触发 | 200ms 最小间隔 + BPM 范围限制（40~200） |
| 串口数据不可读 | 双模式输出（人类模式 / 数据模式） |

### 加分项

1. **数据对比验证**：将 CSV 导出的数据与标准 MIT-BIH 数据库的 PPG 信号对比
2. **硬件外壳**：用 3D 打印或纸板做一个指夹，让传感器固定在手指上
3. **双核利用**：讲述 ESP32 双核架构的优势（Core 0 采样，Core 1 处理通信）
4. **文档意识**：展示你记录的开发过程和问题排查思路

### 完整项目文件清单

```
ESP32-HeartRate-SpO2/
├── platformio.ini              ← PlatformIO 项目配置
├── src/
│   └── main.cpp                ← ESP32 主程序
├── lib/
│   ├── Filter/                 ← 数字滤波器库
│   │   ├── Filter.h
│   │   └── Filter.cpp
│   ├── HeartRate/              ← 心率检测库
│   │   ├── HeartRate.h
│   │   └── HeartRate.cpp
│   └── SpO2/                   ← 血氧计算库
│       ├── SpO2.h
│       └── SpO2.cpp
├── pc_app/                     ← Python 上位机
│   ├── requirements.txt
│   └── monitor.py
├── README.md                   ← 项目简介
└── Tutorial.md                 ← 你正在读的这个教程
```
