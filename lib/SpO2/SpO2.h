/**
 * SpO2.h - 血氧饱和度计算模块
 *
 * 原理：基于朗伯-比尔定律（Beer-Lambert Law）
 *
 * MAX30102 交替发射红光（660nm）和红外光（880nm），
 * 含氧血红蛋白和脱氧血红蛋白对这两种光的吸收率不同：
 *   - 含氧血红蛋白（HbO2）：红外光吸收率高
 *   - 脱氧血红蛋白（Hb）  ：红光吸收率高
 *
 * 通过计算两种光的 AC/DC 比值（R 值），可以推算血氧：
 *   R = (AC_red / DC_red) / (AC_ir / DC_ir)
 *   SpO2 ≈ 110 - 25 * R  （经验线性近似公式）
 *
 * 其中：
 *   AC = 信号中随脉搏跳动变化的部分（交流分量）= 峰值 - 谷值
 *   DC = 信号的基线水平（直流分量）= 一段时间内的平均值
 */

#ifndef SPO2_H
#define SPO2_H

#include <Arduino.h>

class SpO2Calculator {
public:
    SpO2Calculator();

    // 每个采样周期调用，传入滤波后的红光和红外光值
    void update(uint32_t redValue, uint32_t irValue);

    // 通知计算器：心率模块刚检测到一个峰值
    // 这意味着一个完整的心跳周期结束了，可以结算 AC/DC
    void onBeatDetected();

    // 获取当前血氧值（%），数据不足返回 0
    int getSpO2();

    // 获取当前 R 值（用于调试）
    float getRValue();

    // 重置
    void reset();

private:
    // ---------- 当前心跳周期内的追踪 ----------
    uint32_t _redMax, _redMin;   // 红光在本周期内的最大最小值
    uint32_t _irMax, _irMin;     // 红外光在本周期内的最大最小值
    uint32_t _redSum, _irSum;    // 累加值（用于计算 DC 平均值）
    uint32_t _sampleCount;       // 本周期的采样点数

    // ---------- 计算结果 ----------
    float _rValue;               // R = (AC_red/DC_red) / (AC_ir/DC_ir)
    int   _spo2;                 // 血氧饱和度百分比

    // ---------- 平滑输出 ----------
    float _spo2Sum;              // 最近几次的 SpO2 累加
    uint8_t _spo2Count;          // 累加次数
    int   _spo2Avg;              // 平均 SpO2

    // 重置单个周期的追踪状态
    void resetCycle();
};

#endif
