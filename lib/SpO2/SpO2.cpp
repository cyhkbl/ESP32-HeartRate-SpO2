/**
 * SpO2.cpp - 血氧饱和度计算实现
 *
 * 一个心跳周期内的信号示意：
 *
 *   信号值
 *     │   ╱╲  ← 峰值（Peak）
 *     │  ╱  ╲
 *     │ ╱    ╲           AC = Peak - Valley（交流分量）
 *     │╱      ╲          DC = 平均值（直流分量）
 *     │        ╲╱  ← 谷值（Valley）
 *     └────────────→ 时间
 *     |← 一个心跳 →|
 */

#include "SpO2.h"

// SpO2 经验公式系数
// SpO2 = SPO2_COEFF_A - SPO2_COEFF_B * R
// 这个公式来自对大量临床数据的线性拟合
// 不同论文/厂商的系数略有不同，110 和 25 是最常用的近似值
#define SPO2_COEFF_A 110.0f
#define SPO2_COEFF_B 25.0f

// 平均多少个心跳周期的 SpO2 来输出
#define SPO2_AVG_COUNT 4

SpO2Calculator::SpO2Calculator() {
    reset();
}

void SpO2Calculator::reset() {
    resetCycle();
    _rValue = 0.0f;
    _spo2 = 0;
    _spo2Sum = 0.0f;
    _spo2Count = 0;
    _spo2Avg = 0;
}

void SpO2Calculator::resetCycle() {
    _redMax = 0;
    _redMin = UINT32_MAX;
    _irMax = 0;
    _irMin = UINT32_MAX;
    _redSum = 0;
    _irSum = 0;
    _sampleCount = 0;
}

void SpO2Calculator::update(uint32_t redValue, uint32_t irValue) {
    // 追踪本心跳周期内的最大值、最小值和累加值

    // 更新红光的最大最小值
    if (redValue > _redMax) _redMax = redValue;
    if (redValue < _redMin) _redMin = redValue;

    // 更新红外光的最大最小值
    if (irValue > _irMax) _irMax = irValue;
    if (irValue < _irMin) _irMin = irValue;

    // 累加（用于计算 DC 均值）
    _redSum += redValue;
    _irSum += irValue;
    _sampleCount++;
}

void SpO2Calculator::onBeatDetected() {
    // 当心率模块检测到一个峰值时调用
    // 说明一个完整的心跳周期结束了，可以计算这个周期的 SpO2

    // 至少需要 10 个采样点才能可靠计算
    if (_sampleCount < 10) {
        resetCycle();
        return;
    }

    // 计算 DC 分量（平均值）
    float dcRed = (float)_redSum / _sampleCount;
    float dcIr  = (float)_irSum / _sampleCount;

    // 防止除以零
    if (dcRed < 1.0f || dcIr < 1.0f) {
        resetCycle();
        return;
    }

    // 计算 AC 分量（峰值 - 谷值）
    float acRed = (float)(_redMax - _redMin);
    float acIr  = (float)(_irMax - _irMin);

    // AC 分量太小说明信号质量差（手指没放好或没有脉搏波动）
    if (acRed < 1.0f || acIr < 1.0f) {
        resetCycle();
        return;
    }

    // ========== 核心公式 ==========
    // R = (AC_red / DC_red) / (AC_ir / DC_ir)
    //
    // 物理含义：
    // AC/DC 比值反映了脉搏波动在总信号中的占比
    // R 值比较了红光和红外光的这个占比
    //
    // R ≈ 1.0 时，SpO2 ≈ 85%
    // R ≈ 0.4 时，SpO2 ≈ 100%
    // R 越小，血氧越高
    float ratioRed = acRed / dcRed;
    float ratioIr  = acIr / dcIr;
    _rValue = ratioRed / ratioIr;

    // SpO2 = 110 - 25 * R
    float spo2 = SPO2_COEFF_A - SPO2_COEFF_B * _rValue;

    // 限制在合理范围内
    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 0.0f) spo2 = 0.0f;

    _spo2 = (int)spo2;

    // 累加到平均值中
    _spo2Sum += spo2;
    _spo2Count++;

    if (_spo2Count >= SPO2_AVG_COUNT) {
        _spo2Avg = (int)(_spo2Sum / _spo2Count);
        _spo2Sum = 0.0f;
        _spo2Count = 0;
    }

    // 重置，开始追踪下一个心跳周期
    resetCycle();
}

int SpO2Calculator::getSpO2() {
    // 优先返回多次平均后的稳定值
    if (_spo2Avg > 0) {
        return _spo2Avg;
    }
    // 如果还没攒够平均次数，返回最近单次的值
    return _spo2;
}

float SpO2Calculator::getRValue() {
    return _rValue;
}
