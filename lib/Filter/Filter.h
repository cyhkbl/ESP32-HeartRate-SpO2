/**
 * Filter.h - 数字滤波器模块
 *
 * 原理：传感器原始信号包含噪声（手指抖动、环境光干扰等）。
 * 滤波器的作用是平滑信号，去除高频噪声，保留有用的脉搏波形。
 *
 * 实现了两种经典滤波器：
 *   1. 均值滤波（Moving Average）：计算滑动窗口内的平均值
 *   2. 中值滤波（Median Filter）：取滑动窗口内的中位数
 */

#ifndef FILTER_H
#define FILTER_H

#include <Arduino.h>

// 滤波器窗口大小（即用最近多少个采样点来做平滑）
// 值越大越平滑，但延迟也越大。4 是一个平衡的选择。
#define FILTER_WINDOW_SIZE 4

class MeanFilter {
public:
    MeanFilter();

    // 输入一个新的原始值，返回滤波后的值
    uint32_t update(uint32_t newValue);

    // 重置滤波器状态
    void reset();

private:
    uint32_t _buffer[FILTER_WINDOW_SIZE];  // 环形缓冲区，存储最近的采样值
    uint8_t  _index;                        // 当前写入位置
    uint8_t  _count;                        // 已填入的数据个数（用于启动阶段）
    uint32_t _sum;                          // 缓冲区内所有值的总和（避免每次重新求和）
};

class MedianFilter {
public:
    MedianFilter();

    // 输入一个新的原始值，返回滤波后的值
    uint32_t update(uint32_t newValue);

    // 重置滤波器状态
    void reset();

private:
    uint32_t _buffer[FILTER_WINDOW_SIZE];  // 环形缓冲区
    uint8_t  _index;
    uint8_t  _count;
};

#endif
