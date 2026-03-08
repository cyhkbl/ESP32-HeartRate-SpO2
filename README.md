# ESP32 + MAX30102 心率血氧监测系统

基于 ESP32 和 MAX30102 光学传感器的便携式心率（BPM）与血氧饱和度（SpO2）实时监测系统。

## 系统架构

```
┌──────────┐   I2C    ┌──────────────────┐   USB 串口   ┌────────────────┐
│ MAX30102 │ ───────→ │     ESP32        │ ──────────→ │  PC 上位机      │
│ 光学传感器│          │                  │             │  (Python)       │
│          │          │  中值滤波         │             │                │
│ 红光 660nm│          │  均值滤波         │             │  实时 PPG 波形  │
│ 红外 880nm│          │  峰值检测 → BPM  │             │  BPM / SpO2    │
│          │          │  AC/DC → SpO2    │             │  CSV 数据导出   │
└──────────┘          └──────────────────┘             └────────────────┘
```

## 核心算法

| 模块 | 算法 | 说明 |
|------|------|------|
| 数字滤波 | 中值滤波 + 均值滤波（两级串联） | 中值去毛刺，均值平滑高频噪声 |
| 心率检测 | 动态阈值峰值检测 | 自适应阈值跟踪信号幅度变化，拐点判定心跳 |
| 血氧计算 | Beer-Lambert 定律 | R = (AC_red/DC_red) / (AC_ir/DC_ir)，SpO2 = 110 - 25R |

## 硬件需求

| 硬件 | 说明 |
|------|------|
| ESP32 开发板 | 任意型号（ESP32-DevKitC、ESP32-WROOM 等） |
| MAX30102 模块 | 双波长（红光 + 红外光）脉搏血氧传感器 |
| 杜邦线 4 根 | VIN→3.3V, GND→GND, SDA→GPIO21, SCL→GPIO22 |

## 项目结构

```
├── platformio.ini              # PlatformIO 项目配置
├── src/
│   ├── main.cpp                # ESP32 主程序（传感器驱动 + 流水线调度 + 双模式串口输出）
│   └── mobile_web.cpp          # 手机演示固件（ESP32 热点 + Web 实时波形）
├── lib/
│   ├── Filter/                 # 数字滤波器（均值滤波 & 中值滤波）
│   │   ├── Filter.h
│   │   └── Filter.cpp
│   ├── HeartRate/              # 心率检测（动态阈值峰值检测 + R-R 间期计算）
│   │   ├── HeartRate.h
│   │   └── HeartRate.cpp
│   └── SpO2/                   # 血氧计算（AC/DC 分量提取 + R 值计算）
│       ├── SpO2.h
│       └── SpO2.cpp
├── pc_app/                     # Python PC 上位机
│   ├── requirements.txt
│   └── monitor.py              # 串口通信 + 实时 PPG 波形绑制 + CSV 导出
├── Tutorial.md                 # 从零开始的完整教程（三阶段手把手教学）
└── README.md
```

## 快速开始

### 1. ESP32 固件

前提：已安装 [VSCode](https://code.visualstudio.com/) 和 [PlatformIO](https://platformio.org/install/ide?install=vscode) 插件。

```bash
# 用 VSCode 打开项目文件夹后：
# 底部状态栏点击 → (Upload) 编译并烧录
# 底部状态栏点击插头图标 (Serial Monitor) 查看输出
```

烧录成功后，串口监视器将每秒输出一次心率和血氧数据：

```
----------------------------------------
  原始值   Red: 102345  IR: 115678
  滤波后   Red: 102301  IR: 115623
  心率:    72 BPM  (R-R间期: 833 ms)
  血氧:    97 %  (R值: 0.520)
  累计心跳: 15 次
```

### 2. PC 上位机

前提：已安装 Python 3.8+。运行前需**关闭 PlatformIO 的串口监视器**（不能同时占用串口）。

```bash
cd pc_app
pip install -r requirements.txt
python monitor.py          # 自动检测串口
python monitor.py COM3     # 或手动指定串口
```

上位机会自动切换 ESP32 到数据模式，弹出实时 PPG 波形窗口，关闭时自动将数据导出为 CSV 文件。

### 3. 手机网页演示

#### 烧录步骤

1. 在 PlatformIO 里选择环境 `esp32_mobile_web`
2. 点击 Upload 烧录
3. 打开串口监视器（115200）确认启动日志中出现 AP 信息

#### 演示步骤

1. 手机连接 ESP32 热点：`ESP32-HealthDemo`
2. 密码：`12345678`
3. 手机浏览器访问：`http://192.168.4.1`
4. 将手指轻放在 MAX30102 上，页面会显示：
   - 红光/红外 PPG 实时波形
   - 心率 BPM
   - 血氧 SpO2

> 注意：如果页面显示“等待手指”，请检查手指是否覆盖传感器，并保持 5~10 秒稳定接触。

## 技术栈

| 层级 | 技术 |
|------|------|
| 固件 | C++ / Arduino 框架 / PlatformIO |
| 通信 | I2C（传感器）、UART 串口（PC） |
| 上位机 | Python / pyserial / matplotlib |
| 移动演示 | ESP32 SoftAP / WebServer / HTML Canvas |
