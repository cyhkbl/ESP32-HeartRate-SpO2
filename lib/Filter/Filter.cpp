/**
 * Filter.cpp - 数字滤波器实现
 */

#include "Filter.h"

// ==================== 均值滤波器 ====================
//
// 工作原理（举例，窗口大小=4）：
//
//   时刻1: 输入 100        缓冲区 [100, -, -, -]     输出 100/1 = 100
//   时刻2: 输入 104        缓冲区 [100, 104, -, -]   输出 204/2 = 102
//   时刻3: 输入 98         缓冲区 [100, 104, 98, -]  输出 302/3 ≈ 100
//   时刻4: 输入 102        缓冲区 [100, 104, 98, 102] 输出 404/4 = 101
//   时刻5: 输入 200(噪声)  缓冲区 [200, 104, 98, 102] 输出 504/4 = 126
//
// 可以看到，单个异常值 200 被平滑成了 126，噪声被大幅削弱。

MeanFilter::MeanFilter() {
    reset();
}

void MeanFilter::reset() {
    _index = 0;
    _count = 0;
    _sum = 0;
    memset(_buffer, 0, sizeof(_buffer));
}

uint32_t MeanFilter::update(uint32_t newValue) {
    // 如果缓冲区已满，减去即将被覆盖的旧值
    if (_count >= FILTER_WINDOW_SIZE) {
        _sum -= _buffer[_index];
    } else {
        _count++;
    }

    // 写入新值
    _buffer[_index] = newValue;
    _sum += newValue;

    // 环形索引：到达末尾后回到开头
    _index = (_index + 1) % FILTER_WINDOW_SIZE;

    // 返回平均值
    return _sum / _count;
}

// ==================== 中值滤波器 ====================
//
// 工作原理（举例，窗口大小=4）：
//
//   缓冲区 [100, 200, 98, 102]
//   排序后 [98, 100, 102, 200]
//   中位数 = (100 + 102) / 2 = 101
//
// 异常值 200 完全不影响输出！这就是中值滤波对毛刺的抵抗力。

MedianFilter::MedianFilter() {
    reset();
}

void MedianFilter::reset() {
    _index = 0;
    _count = 0;
    memset(_buffer, 0, sizeof(_buffer));
}

uint32_t MedianFilter::update(uint32_t newValue) {
    // 写入新值到环形缓冲区
    _buffer[_index] = newValue;
    _index = (_index + 1) % FILTER_WINDOW_SIZE;
    if (_count < FILTER_WINDOW_SIZE) {
        _count++;
    }

    // 把当前缓冲区的有效数据复制出来排序（不能直接排序缓冲区，会破坏顺序）
    uint32_t sorted[FILTER_WINDOW_SIZE];
    for (uint8_t i = 0; i < _count; i++) {
        sorted[i] = _buffer[i];
    }

    // 简单插入排序（数据量很小，插入排序效率足够）
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
    if (_count % 2 == 0) {
        // 偶数个：取中间两个数的平均
        return (sorted[_count / 2 - 1] + sorted[_count / 2]) / 2;
    } else {
        // 奇数个：直接取中间那个
        return sorted[_count / 2];
    }
}
