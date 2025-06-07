// metrics.hpp
#pragma once
#include <atomic>
#include <string>
#include <map>
#include <iostream>

class Metrics {
public:
    static void inc(const std::string& name, int value = 1) {
        counters()[name].fetch_add(value, std::memory_order_relaxed);
    }

    static void dec(const std::string& name, int value = 1) {
        counters()[name].fetch_sub(value, std::memory_order_relaxed);
    }

    static int get(const std::string& name) {
        return counters()[name].load(std::memory_order_relaxed);
    }

    static void dump_all() {
        for (const auto& [name, counter] : counters()) {
            std::cout << name << ": " << counter.load() << std::endl;
        }
    }

private:
    static std::map<std::string, std::atomic<int>>& counters() {
        static std::map<std::string, std::atomic<int>> inst;
        return inst;
    }
};