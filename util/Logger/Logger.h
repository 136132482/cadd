#pragma once

#include <string>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <memory>
#include <fstream>

class Logger {
public:
    // 日志级别枚举
    enum class Level {
        None = 0,
        Error = 1,
        Warning = 2,
        Info = 3,
        Debug = 4,
        Trace = 5
    };

    // 输出目标枚举
    enum class Target {
        Console,
        File,
        Both
    };

    // 静态方法 - 全局日志配置
    static void SetGlobalLevel(Level level);
    static void SetGlobalTarget(Target target);
    static void SetGlobalFile(const std::string& filename);

    // 获取全局Logger实例
    static Logger& Global();

    // 构造函数
    explicit Logger(const std::string& name = "");
    Logger(const std::string& name, Level level, Target target, const std::string& filename = "");

    // 实例方法 - 日志输出
    void Trace(const std::string& message) const;
    void Debug(const std::string& message) const;
    void Info(const std::string& message) const;
    void Warning(const std::string& message) const;
    void Error(const std::string& message) const;

    // 实例方法 - 配置
    void SetLevel(Level level);
    void SetTarget(Target target);
    void SetFile(const std::string& filename);

    // 格式化输出
    // 格式化输出模板实现
    template<typename... Args>
    void TraceFmt(const std::string& format, Args... args) const {
        if (Level::Trace <= level_) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            Trace(buffer);
        }
    }

    template<typename... Args>
    void DebugFmt(const std::string& format, Args... args) const {
        if (Level::Debug <= level_) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            Debug(buffer);
        }
    }

    template<typename... Args>
    void InfoFmt(const std::string& format, Args... args) const {
        if (Level::Info <= level_) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            Info(buffer);
        }
    }

    template<typename... Args>
    void WarningFmt(const std::string& format, Args... args) const {
        if (Level::Warning <= level_) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            Warning(buffer);
        }
    }

    template<typename... Args>
    void ErrorFmt(const std::string& format, Args... args) const {
        if (Level::Error <= level_) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            Error(buffer);
        }
    }

private:
    // 内部实现
    void Log(Level level, const std::string& message) const;
    static std::string LevelToString(Level level);
    static std::string GetCurrentTime();

    // 实例成员
    std::string name_;
    Level level_;
    Target target_;
    mutable std::mutex mutex_;
    std::unique_ptr<std::ofstream> file_;
    std::string filename_;

    // 静态成员
    static std::unique_ptr<Logger> globalLogger_;
    static Level globalLevel_;
    static Target globalTarget_;
    static std::string globalFilename_;
    static std::mutex globalMutex_;
};