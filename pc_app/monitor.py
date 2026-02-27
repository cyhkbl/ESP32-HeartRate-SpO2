"""
ESP32 + MAX30102 心率血氧监测 - PC 上位机
功能：
  1. 通过串口接收 ESP32 发送的数据
  2. 实时绘制 PPG（光电容积脉搏波）波形
  3. 实时显示心率（BPM）和血氧饱和度（SpO2%）
  4. 关闭时自动将数据导出为 CSV 文件

用法：
  python monitor.py              # 自动检测串口
  python monitor.py COM3         # 指定串口（Windows）
  python monitor.py /dev/ttyUSB0 # 指定串口（Linux/Mac）
"""

import serial
import serial.tools.list_ports
import threading
import time
import csv
import sys
import os
from collections import deque
from datetime import datetime

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# ==================== 配置 ====================

BAUD_RATE = 115200       # 波特率（必须和 ESP32 代码中的一致）
PLOT_WINDOW = 300        # 波形图显示最近多少个数据点（约 6 秒）
ANIMATION_INTERVAL = 50  # 图表刷新间隔（毫秒）

# ==================== 全局数据 ====================

# deque 是一个固定长度的队列，超出长度时自动丢弃最旧的数据
# 这样波形图就像一个滑动窗口，始终显示最近的数据
time_data = deque(maxlen=PLOT_WINDOW)
red_data = deque(maxlen=PLOT_WINDOW)
ir_data = deque(maxlen=PLOT_WINDOW)

# 当前生理参数（用列表包装以便在线程间共享）
current_bpm = [0]
current_spo2 = [0]
beat_flag = [False]

# 所有接收到的数据（用于 CSV 导出）
all_records = []

# 程序运行标志
running = True

# 无手指标志
no_finger = [False]


# ==================== 串口相关 ====================

def find_serial_port():
    """自动检测可用的串口设备"""

    # 如果用户在命令行指定了串口，直接使用
    if len(sys.argv) > 1:
        port = sys.argv[1]
        print(f"使用指定串口: {port}")
        return port

    # 否则自动检测
    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("错误: 没有检测到任何串口设备!")
        print("请确认:")
        print("  1. ESP32 已通过 USB 数据线连接到电脑")
        print("  2. 已安装 ESP32 的 USB 驱动（CP2102 或 CH340）")
        print("  3. 没有其他程序正在占用该串口（如 Arduino 串口监视器）")
        sys.exit(1)

    if len(ports) == 1:
        port = ports[0].device
        print(f"自动检测到串口: {port} ({ports[0].description})")
        return port

    # 多个串口，让用户选择
    print("检测到多个串口设备:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")

    while True:
        try:
            choice = input("请输入串口编号: ").strip()
            idx = int(choice)
            if 0 <= idx < len(ports):
                return ports[idx].device
        except (ValueError, KeyboardInterrupt):
            pass
        print("输入无效，请重新选择。")


def serial_reader(ser):
    """
    后台线程函数：持续从串口读取数据并解析

    ESP32 在数据模式下发送的格式：
      DATA,timestamp_ms,filteredRed,filteredIR,bpm,spo2,beat(0或1)

    例如：
      DATA,12345,102301,115623,72,97,0
      DATA,12365,102298,115610,72,97,1  ← beat=1 表示检测到一次心跳
    """
    global running

    while running:
        try:
            # 读取一行（以 \n 结尾）
            raw_line = ser.readline()
            if not raw_line:
                continue

            line = raw_line.decode("utf-8", errors="ignore").strip()

            # 无手指标志
            if line == "NOFINGER":
                no_finger[0] = True
                continue
            else:
                no_finger[0] = False

            # 只解析 DATA 开头的行
            if not line.startswith("DATA,"):
                continue

            # 解析 CSV 字段
            parts = line[5:].split(",")
            if len(parts) != 6:
                continue

            t_ms = int(parts[0])
            red = int(parts[1])
            ir = int(parts[2])
            bpm = int(parts[3])
            spo2 = int(parts[4])
            beat = int(parts[5])

            # 更新绘图数据
            time_data.append(t_ms / 1000.0)  # 毫秒转换为秒
            red_data.append(red)
            ir_data.append(ir)

            # 更新生理参数
            current_bpm[0] = bpm
            current_spo2[0] = spo2
            if beat:
                beat_flag[0] = True

            # 保存原始记录（用于 CSV 导出）
            all_records.append([t_ms, red, ir, bpm, spo2, beat])

        except serial.SerialException:
            print("\n串口连接断开!")
            running = False
            break
        except Exception:
            # 解析错误时跳过这一行
            continue


# ==================== CSV 导出 ====================

def save_csv():
    """将采集到的所有数据保存为 CSV 文件"""

    if not all_records:
        print("没有采集到数据，跳过 CSV 导出。")
        return

    # 文件名包含时间戳，避免覆盖
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"ppg_data_{timestamp}.csv"
    filepath = os.path.join(os.path.dirname(os.path.abspath(__file__)), filename)

    with open(filepath, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        # 写入表头
        writer.writerow(["timestamp_ms", "red", "ir", "bpm", "spo2", "beat"])
        # 写入数据
        writer.writerows(all_records)

    print(f"数据已保存: {filepath} (共 {len(all_records)} 条记录)")


# ==================== 图表绘制 ====================

def setup_plot():
    """
    创建 matplotlib 图表

    布局：
    ┌─────────────────────────────────────────────┐
    │   心率: 72 BPM    血氧: 97%            ♥    │
    ├─────────────────────────────────────────────┤
    │  红光 PPG 波形 (660nm)                      │
    │  ~~~~∿~~~~∿~~~~∿~~~~                       │
    ├─────────────────────────────────────────────┤
    │  红外光 PPG 波形 (880nm)                    │
    │  ~~~~∿~~~~∿~~~~∿~~~~                       │
    └─────────────────────────────────────────────┘
    """

    # 设置中文字体（Windows 常用字体）
    plt.rcParams["font.sans-serif"] = [
        "SimHei",            # 黑体
        "Microsoft YaHei",   # 微软雅黑
        "Arial Unicode MS",  # Mac 备选
        "DejaVu Sans",       # Linux 备选
    ]
    plt.rcParams["axes.unicode_minus"] = False  # 解决负号显示问题

    fig, (ax_red, ax_ir) = plt.subplots(2, 1, figsize=(10, 6))

    fig.suptitle(
        "心率: -- BPM    血氧: -- %",
        fontsize=16,
        fontweight="bold",
    )
    fig.subplots_adjust(hspace=0.35, top=0.88, bottom=0.08)

    # 红光波形
    (line_red,) = ax_red.plot([], [], color="red", linewidth=1)
    ax_red.set_title("红光 PPG 波形 (660nm)", fontsize=11)
    ax_red.set_ylabel("信号强度")
    ax_red.grid(True, alpha=0.3)

    # 红外光波形
    (line_ir,) = ax_ir.plot([], [], color="purple", linewidth=1)
    ax_ir.set_title("红外光 PPG 波形 (880nm)", fontsize=11)
    ax_ir.set_xlabel("时间 (秒)")
    ax_ir.set_ylabel("信号强度")
    ax_ir.grid(True, alpha=0.3)

    # 心跳指示符号（右上角闪烁的 ♥）
    beat_text = fig.text(
        0.95, 0.92, "", fontsize=24, ha="center", va="center",
        color="red", fontweight="bold",
    )

    # 状态文字（无手指时显示提示）
    status_text = fig.text(
        0.5, 0.5, "", fontsize=14, ha="center", va="center",
        color="gray", fontstyle="italic",
        transform=fig.transFigure,
    )

    return fig, ax_red, ax_ir, line_red, line_ir, beat_text, status_text


def create_updater(fig, ax_red, ax_ir, line_red, line_ir, beat_text, status_text):
    """创建动画更新函数"""

    def update(frame):
        # 无手指提示
        if no_finger[0]:
            status_text.set_text("请将手指放在传感器上...")
            fig.suptitle(
                "心率: -- BPM    血氧: -- %",
                fontsize=16, fontweight="bold",
            )
            return line_red, line_ir, beat_text, status_text

        status_text.set_text("")

        if not time_data:
            return line_red, line_ir, beat_text, status_text

        # 取出当前数据（转为列表避免迭代时被修改）
        t = list(time_data)
        r = list(red_data)
        ir = list(ir_data)

        # 更新红光波形
        line_red.set_data(t, r)
        ax_red.set_xlim(t[0], t[-1])
        if r:
            r_min, r_max = min(r), max(r)
            margin = max((r_max - r_min) * 0.1, 1)
            ax_red.set_ylim(r_min - margin, r_max + margin)

        # 更新红外光波形
        line_ir.set_data(t, ir)
        ax_ir.set_xlim(t[0], t[-1])
        if ir:
            ir_min, ir_max = min(ir), max(ir)
            margin = max((ir_max - ir_min) * 0.1, 1)
            ax_ir.set_ylim(ir_min - margin, ir_max + margin)

        # 更新标题栏的心率和血氧
        bpm = current_bpm[0]
        spo2 = current_spo2[0]
        bpm_str = str(bpm) if bpm > 0 else "--"
        spo2_str = str(spo2) if spo2 > 0 else "--"
        fig.suptitle(
            f"心率: {bpm_str} BPM    血氧: {spo2_str} %",
            fontsize=16, fontweight="bold",
        )

        # 心跳指示：检测到心跳时闪烁 ♥
        if beat_flag[0]:
            beat_text.set_text("♥")
            beat_flag[0] = False
        else:
            beat_text.set_text("")

        return line_red, line_ir, beat_text, status_text

    return update


# ==================== 主程序 ====================

def main():
    global running

    print("=" * 50)
    print("  ESP32 + MAX30102 心率血氧监测 - PC 上位机")
    print("=" * 50)
    print()

    # 1. 查找并连接串口
    port = find_serial_port()
    print(f"正在连接 {port} ...")

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
    except serial.SerialException as e:
        print(f"无法打开串口: {e}")
        print("请确认没有其他程序占用该串口（如 PlatformIO 串口监视器）。")
        sys.exit(1)

    # 等待 ESP32 启动（USB 连接会触发 ESP32 复位）
    print("等待 ESP32 启动...")
    time.sleep(2)

    # 2. 切换 ESP32 到数据模式
    ser.write(b"D")
    time.sleep(0.1)
    print("已切换为数据传输模式")
    print()
    print("操作提示:")
    print("  - 将手指轻放在传感器上，保持不动")
    print("  - 关闭图表窗口退出程序")
    print("  - 退出时自动保存 CSV 数据文件")
    print()

    # 3. 启动后台串口读取线程
    reader_thread = threading.Thread(target=serial_reader, args=(ser,), daemon=True)
    reader_thread.start()

    # 4. 创建实时图表
    fig, ax_red, ax_ir, line_red, line_ir, beat_text, status_text = setup_plot()
    update_func = create_updater(
        fig, ax_red, ax_ir, line_red, line_ir, beat_text, status_text,
    )

    ani = animation.FuncAnimation(
        fig,
        update_func,
        interval=ANIMATION_INTERVAL,
        blit=False,
        cache_frame_data=False,
    )

    # 5. 显示图表（阻塞直到窗口关闭）
    try:
        plt.show()
    except KeyboardInterrupt:
        pass

    # 6. 清理退出
    running = False
    print()
    print("正在关闭...")

    # 切换 ESP32 回人类模式
    try:
        ser.write(b"H")
        time.sleep(0.1)
        ser.close()
    except Exception:
        pass

    # 自动保存 CSV
    save_csv()
    print("程序已退出。")


if __name__ == "__main__":
    main()
