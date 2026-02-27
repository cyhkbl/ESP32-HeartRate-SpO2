/**
 * HeartRate.h - 心率检测模块
 *
 * 原理：通过检测 PPG（光电容积脉搏波）信号中的峰值，
 *       计算相邻峰值之间的时间间隔（R-R 间期），
 *       从而推算心率（BPM = 60000 / R-R间期毫秒数）。
 *
 * 峰值检测算法：
 *   1. 维护一个动态阈值（信号最大值和最小值的中点）
 *   2. 当信号从低于阈值上升到高于阈值时，标记"可能有峰"
 *   3. 在上升阶段结束（信号开始下降）时，确认峰值
 *   4. 记录峰值的时间戳，与上一个峰值做差得到 R-R 间期
 */

#ifndef HEARTRATE_H
#define HEARTRATE_H

#include <Arduino.h>

// 存储最近多少个 R-R 间期来计算平均心率
// 用多个间期取平均可以让心率显示更稳定
#define RR_BUFFER_SIZE 4

// 有效心率范围（过滤明显不合理的值）
#define BPM_MIN 40    // 最低 40 BPM（运动员静息可能很低）
#define BPM_MAX 200   // 最高 200 BPM（剧烈运动）

// 两次峰值之间的最小间隔（毫秒）
// 200ms 对应 300 BPM，防止把噪声抖动误判为心跳
#define PEAK_MIN_INTERVAL_MS 200

class HeartRateDetector {
public:
    HeartRateDetector();

    // 输入一个滤波后的 IR 值，内部自动检测峰值和计算心率
    // 返回 true 表示检测到了新的峰值（心跳一次）
    bool update(uint32_t irValue);

    // 获取当前心率（BPM），如果数据不足返回 0
    int getBPM();

    // 获取最近一次的 R-R 间期（毫秒）
    unsigned long getLastRRInterval();

    // 重置所有状态
    void reset();

private:
    // ---------- 峰值检测状态 ----------
    uint32_t _prevValue;          // 上一次的信号值（用于判断上升/下降）
    bool     _rising;             // 当前信号是否处于上升阶段
    uint32_t _peakValue;          // 当前上升阶段的最大值
    unsigned long _lastPeakTime;  // 上一个峰值的时间戳

    // ---------- 动态阈值 ----------
    uint32_t _signalMax;          // 近期信号最大值
    uint32_t _signalMin;          // 近期信号最小值
    uint32_t _threshold;          // 动态阈值 = (max + min) / 2
    bool     _aboveThreshold;     // 信号当前是否在阈值之上

    // ---------- R-R 间期缓冲区 ----------
    unsigned long _rrIntervals[RR_BUFFER_SIZE];  // 存储最近的 R-R 间期
    uint8_t _rrIndex;             // 环形缓冲区写入位置
    uint8_t _rrCount;             // 已有的有效间期数

    // 更新动态阈值
    void updateThreshold(uint32_t value);
};

#endif
