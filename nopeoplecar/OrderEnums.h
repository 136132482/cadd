// OrderEnums.h
#pragma once
#include "../util/enum/GenericEnum.h"
#include <random>
#include <mutex>


// Thread-safe random number generator
class ThreadSafeRandom {
    std::random_device rd;
    std::mt19937 gen;
    std::mutex mtx;
public:
    ThreadSafeRandom() : gen(rd()) {}

    int operator()(int min, int max) {
        std::lock_guard<std::mutex> lock(mtx);
        std::uniform_int_distribution<int> dist(min, max);
        return dist(gen);
    }

    double operator()(double min, double max) {
        std::lock_guard<std::mutex> lock(mtx);
        std::uniform_real_distribution<double> dist(min, max);
        return dist(gen);
    }
};
// Global random generator
ThreadSafeRandom safe_random;

// 订单类型枚举
NumberEnum createOrderTypeEnum() {
    return NumberEnum()
            // 格式：.add(编码, {大类, 子类, 显示名称, "能力1,能力2,..."})
            .add(101, {"四轮车", "日常百货", "百货配送车", "保温箱"})
            .add(102, {"四轮车", "餐饮", "餐饮配送车", "保温箱"})
            .add(201, {"无人机", "医药", "医药无人机", "防震/夜视"})
            .add(301, {"四轮车", "快递", "快递车", "防震"})
            .add(401, {"机器人", "电子产品", "电子配送机器人", "防震/防水"})
            .add(501, {"四轮车", "冷藏", "冷藏车", "保温箱/防水"})
            .add(601, {"无人机", "文件", "文件无人机", "防震"})
            .add(701, {"四轮车", "鲜花", "鲜花配送车", "保温箱"})
            .add(801, {"机器人", "服装", "服装配送机器人", "防震"})
            .add(901, {"四轮车", "图书", "图书配送车", "防震"});
}

// 订单状态枚举
NumberEnum createOrderStatusEnum() {
    return NumberEnum()
            .add(0, {"待处理", "pending"})
            .add(1, {"已接单", "accepted"})
            .add(2, {"配送中", "delivering"})
            .add(3, {"已完成", "completed"})
            .add(4, {"已取消", "canceled"});
}

// 全局枚举对象
//extern NumberEnum OrderTypeEnum;
//extern NumberEnum OrderStatusEnum;

NumberEnum OrderTypeEnum = createOrderTypeEnum();
NumberEnum OrderStatusEnum = createOrderStatusEnum();