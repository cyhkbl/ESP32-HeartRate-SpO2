/**
 * HeartRate.cpp - 心率检测实现
 *
 * 峰值检测的完整流程：
 *
 *   信号值
 *     │    ╱╲
 *     │   ╱  ╲       ← 信号上升到峰值后开始下降
 *     │  ╱    ╲
 *  阈值├─╱──────╲──   ← 动态阈值（自动调整）
 *     │╱        ╲
 *     └───────────→ 时间
 *          ↑
 *     信号穿过阈值后继续上升，
 *     直到开始下降 → 确认这是一个峰值
 */

#include "HeartRate.h"

HeartRateDetector::HeartRateDetector() {
    reset();
}

void HeartRateDetector::reset() {
    _prevValue = 0;
    _rising = false;
    _peakValue = 0;
    _lastPeakTime = 0;
    _signalMax = 0;
    _signalMin = UINT32_MAX;
    _threshold = 0;
    _aboveThreshold = false;
    _rrIndex = 0;
    _rrCount = 0;
    memset(_rrIntervals, 0, sizeof(_rrIntervals));
}

void HeartRateDetector::updateThreshold(uint32_t value) {
    // 动态追踪信号的最大值和最小值
    // 使用衰减因子让阈值能适应信号幅度的变化
    //（比如手指按压力度改变导致信号幅度变化）

    if (value > _signalMax) {
        _signalMax = value;
    }
    if (value < _signalMin) {
        _signalMin = value;
    }

    // 阈值 = 最大值和最小值的中点
    _threshold = (_signalMax + _signalMin) / 2;

    // 缓慢衰减最大值和最小值，使阈值能适应信号变化
    // 每次调用让 max 向下收缩一点，min 向上收缩一点
    // 衰减速度很慢（每次 0.3%），不会影响正常检测
    _signalMax = _signalMax - (_signalMax >> 8);  // signalMax *= 0.996
    _signalMin = _signalMin + (_signalMin >> 8);  // signalMin *= 1.004（近似）
}

bool HeartRateDetector::update(uint32_t irValue) {
    bool peakDetected = false;
    unsigned long now = millis();

    // 更新动态阈值
    updateThreshold(irValue);

    // 判断信号上升还是下降
    bool currentlyRising = (irValue > _prevValue);

    // 峰值检测的核心逻辑：
    // 条件1：信号刚从"上升"变为"下降"（拐点出现）
    // 条件2：信号在阈值之上（排除低谷处的小波动）
    // 条件3：距离上次峰值超过最小间隔（排除噪声）
    if (_rising && !currentlyRising && _aboveThreshold) {
        unsigned long interval = now - _lastPeakTime;

        if (interval > PEAK_MIN_INTERVAL_MS && _lastPeakTime > 0) {
            // 计算这次间期对应的 BPM，检查是否在合理范围
            int bpm = 60000 / interval;

            if (bpm >= BPM_MIN && bpm <= BPM_MAX) {
                // 有效的心跳！记录 R-R 间期
                _rrIntervals[_rrIndex] = interval;
                _rrIndex = (_rrIndex + 1) % RR_BUFFER_SIZE;
                if (_rrCount < RR_BUFFER_SIZE) {
                    _rrCount++;
                }
                peakDetected = true;
            }
        }
        _lastPeakTime = now;
    }

    // 更新状态
    _aboveThreshold = (irValue > _threshold);
    _rising = currentlyRising;
    _prevValue = irValue;

    return peakDetected;
}

int HeartRateDetector::getBPM() {
    // 至少需要 2 个间期才能给出可靠的心率
    if (_rrCount < 2) {
        return 0;
    }

    // 计算所有有效 R-R 间期的平均值
    unsigned long sum = 0;
    for (uint8_t i = 0; i < _rrCount; i++) {
        sum += _rrIntervals[i];
    }
    unsigned long avgInterval = sum / _rrCount;

    if (avgInterval == 0) {
        return 0;
    }

    return 60000 / avgInterval;
}

unsigned long HeartRateDetector::getLastRRInterval() {
    if (_rrCount == 0) {
        return 0;
    }
    // 返回最近写入的那个间期
    uint8_t lastIndex = (_rrIndex == 0) ? RR_BUFFER_SIZE - 1 : _rrIndex - 1;
    return _rrIntervals[lastIndex];
}
