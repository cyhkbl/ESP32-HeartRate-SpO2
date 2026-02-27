/**
 * ESP32 + MAX30102 心率血氧监测 - 完整版（含上位机通信）
 *
 * 数据处理流水线：
 *   传感器原始数据 → 中值滤波(去毛刺) → 均值滤波(平滑)
 *                                         ↓
 *                                    峰值检测 → 心率(BPM)
 *                                         ↓
 *                                    AC/DC计算 → 血氧(SpO2%)
 *
 * 串口输出模式：
 *   - 人类模式（默认）：每秒打印一次中文摘要，适合在串口监视器查看
 *   - 数据模式（收到 'D'）：50Hz 输出 CSV 数据，供 Python 上位机解析
 *   - 发送 'H' 切回人类模式
 *
 * 硬件接线：
 *   MAX30102 VIN → ESP32 3.3V
 *   MAX30102 GND → ESP32 GND
 *   MAX30102 SDA → ESP32 GPIO 21
 *   MAX30102 SCL → ESP32 GPIO 22
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"

#include "../lib/Filter/Filter.h"
#include "../lib/HeartRate/HeartRate.h"
#include "../lib/SpO2/SpO2.h"

// ==================== 全局对象 ====================

MAX30105 sensor;

MedianFilter medianRed, medianIR;
MeanFilter   meanRed, meanIR;

HeartRateDetector hrDetector;
SpO2Calculator    spo2Calc;

// ==================== 输出模式控制 ====================

bool dataMode = false;  // false=人类模式, true=数据模式

// 人类模式：1 秒打印一次摘要
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL_MS = 1000;

// 数据模式：20ms 输出一次（50Hz）
unsigned long lastDataTime = 0;
const unsigned long DATA_INTERVAL_MS = 20;

// 心跳计数
int beatCount = 0;

// 在数据模式下，心跳标志可能在两次输出之间产生，用 pending 暂存
bool pendingBeat = false;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        delay(10);
    }

    Serial.println("========================================");
    Serial.println("  ESP32 + MAX30102 心率血氧监测系统");
    Serial.println("========================================");
    Serial.println();
    Serial.println("正在初始化传感器...");

    if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("[错误] 未检测到 MAX30102！请检查接线。");
        while (true) { delay(1000); }
    }
    Serial.println("[OK] 传感器连接成功");

    byte ledBrightness = 60;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;

    sensor.setup(ledBrightness, sampleAverage, ledMode,
                 sampleRate, pulseWidth, adcRange);

    Serial.println("[OK] 传感器配置完成");
    Serial.println();
    Serial.println("请将手指轻放在传感器上，保持不动...");
    Serial.println("（提示：发送 'D' 切换到数据模式，'H' 切回人类模式）");
    Serial.println();
}

void loop() {
    // ========== 0. 检查串口命令 ==========
    // Python 上位机连接后会发送 'D' 切换到数据模式
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'D') {
            dataMode = true;
            // 输出表头，让 Python 知道数据格式
            Serial.println("# DATA_MODE_START");
            Serial.println("# timestamp_ms,red,ir,bpm,spo2,beat");
        } else if (cmd == 'H') {
            dataMode = false;
            Serial.println("# HUMAN_MODE_START");
        }
    }

    // ========== 1. 读取原始数据 ==========
    uint32_t rawRed = sensor.getRed();
    uint32_t rawIR  = sensor.getIR();

    unsigned long now = millis();

    // ========== 手指检测 ==========
    if (rawIR < 50000) {
        hrDetector.reset();
        spo2Calc.reset();
        medianRed.reset(); medianIR.reset();
        meanRed.reset();   meanIR.reset();
        beatCount = 0;
        pendingBeat = false;

        if (dataMode) {
            // 数据模式：发送无手指标志
            if (now - lastDataTime >= DATA_INTERVAL_MS) {
                Serial.println("NOFINGER");
                lastDataTime = now;
            }
        } else {
            if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
                Serial.println("[等待] 未检测到手指...");
                lastPrintTime = now;
            }
        }
        delay(10);
        return;
    }

    // ========== 2. 两级滤波 ==========
    uint32_t medRed = medianRed.update(rawRed);
    uint32_t medIR  = medianIR.update(rawIR);

    uint32_t filteredRed = meanRed.update(medRed);
    uint32_t filteredIR  = meanIR.update(medIR);

    // ========== 3. 心率检测 ==========
    bool beatDetected = hrDetector.update(filteredIR);

    // ========== 4. 血氧计算 ==========
    spo2Calc.update(filteredRed, filteredIR);

    if (beatDetected) {
        spo2Calc.onBeatDetected();
        beatCount++;
        pendingBeat = true;  // 标记待输出的心跳
    }

    // ========== 5. 输出 ==========
    if (dataMode) {
        // ---------- 数据模式：50Hz CSV 输出 ----------
        if (now - lastDataTime >= DATA_INTERVAL_MS) {
            Serial.print("DATA,");
            Serial.print(now);
            Serial.print(",");
            Serial.print(filteredRed);
            Serial.print(",");
            Serial.print(filteredIR);
            Serial.print(",");
            Serial.print(hrDetector.getBPM());
            Serial.print(",");
            Serial.print(spo2Calc.getSpO2());
            Serial.print(",");
            Serial.println(pendingBeat ? 1 : 0);
            pendingBeat = false;
            lastDataTime = now;
        }
    } else {
        // ---------- 人类模式：1Hz 可读摘要 ----------
        if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
            int bpm = hrDetector.getBPM();
            int spo2 = spo2Calc.getSpO2();

            Serial.println("----------------------------------------");

            Serial.print("  原始值   Red: ");
            Serial.print(rawRed);
            Serial.print("  IR: ");
            Serial.println(rawIR);

            Serial.print("  滤波后   Red: ");
            Serial.print(filteredRed);
            Serial.print("  IR: ");
            Serial.println(filteredIR);

            Serial.print("  心率:    ");
            if (bpm > 0) {
                Serial.print(bpm);
                Serial.print(" BPM");
                Serial.print("  (R-R间期: ");
                Serial.print(hrDetector.getLastRRInterval());
                Serial.print(" ms)");
            } else {
                Serial.print("计算中...");
            }
            Serial.println();

            Serial.print("  血氧:    ");
            if (spo2 > 0) {
                Serial.print(spo2);
                Serial.print(" %");
                Serial.print("  (R值: ");
                Serial.print(spo2Calc.getRValue(), 3);
                Serial.print(")");
            } else {
                Serial.print("计算中...");
            }
            Serial.println();

            Serial.print("  累计心跳: ");
            Serial.print(beatCount);
            Serial.println(" 次");

            lastPrintTime = now;
        }
    }

    delay(10);
}
