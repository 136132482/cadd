#include "Logger.h"
#include <sstream>
#include <cstdarg>
#include <memory>

// 静态成员初始化
std::unique_ptr<Logger> Logger::globalLogger_;
Logger::Level Logger::globalLevel_ = Logger::Level::Info;
Logger::Target Logger::globalTarget_ = Logger::Target::Console;
std::string Logger::globalFilename_;
std::mutex Logger::globalMutex_;

// 静态方法实现
void Logger::SetGlobalLevel(Level level) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    globalLevel_ = level;
}

void Logger::SetGlobalTarget(Target target) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    globalTarget_ = target;
}

void Logger::SetGlobalFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    globalFilename_ = filename;
}

Logger& Logger::Global() {
    std::lock_guard<std::mutex> lock(globalMutex_);
    if (!globalLogger_) {
        globalLogger_ = std::make_unique<Logger>("Global", globalLevel_, globalTarget_, globalFilename_);
    }
    return *globalLogger_;
}

// 构造函数
Logger::Logger(const std::string& name)
        : name_(name), level_(globalLevel_), target_(globalTarget_), filename_(globalFilename_) {
    if (target_ == Target::File || target_ == Target::Both) {
        file_ = std::make_unique<std::ofstream>(filename_, std::ios::app);
    }
}

Logger::Logger(const std::string& name, Level level, Target target, const std::string& filename)
        : name_(name), level_(level), target_(target), filename_(filename) {
    if (target_ == Target::File || target_ == Target::Both) {
        file_ = std::make_unique<std::ofstream>(filename_, std::ios::app);
    }
}

// 实例方法实现
void Logger::Trace(const std::string& message) const { Log(Level::Trace, message); }
void Logger::Debug(const std::string& message) const { Log(Level::Debug, message); }
void Logger::Info(const std::string& message) const { Log(Level::Info, message); }
void Logger::Warning(const std::string& message) const { Log(Level::Warning, message); }
void Logger::Error(const std::string& message) const { Log(Level::Error, message); }

void Logger::SetLevel(Level level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::SetTarget(Target target) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (target != target_) {
        target_ = target;
        if (target_ == Target::File || target_ == Target::Both) {
            file_ = std::make_unique<std::ofstream>(filename_, std::ios::app);
        } else {
            file_.reset();
        }
    }
}

void Logger::SetFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (filename != filename_) {
        filename_ = filename;
        if (target_ == Target::File || target_ == Target::Both) {
            file_ = std::make_unique<std::ofstream>(filename_, std::ios::app);
        }
    }
}

// 私有方法实现
void Logger::Log(Level level, const std::string& message) const {
    if (level > level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    std::stringstream ss;
    ss << "[" << GetCurrentTime() << "]";
    if (!name_.empty()) ss << "[" << name_ << "]";
    ss << "[" << LevelToString(level) << "] " << message << "\n";

    std::string logMessage = ss.str();

    if (target_ == Target::Console || target_ == Target::Both) {
        std::cout << logMessage;
    }

    if ((target_ == Target::File || target_ == Target::Both) && file_ && file_->is_open()) {
        *file_ << logMessage;
        file_->flush();
    }
}

std::string Logger::LevelToString(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warning: return "WARN";
        case Level::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    return ss.str();
}

