#include "Logger.h"

int main() {
    // 静态全局配置方式
    Logger::SetGlobalLevel(Logger::Level::Debug);
    Logger::SetGlobalTarget(Logger::Target::Both);
    Logger::SetGlobalFile("app.log");

    // 静态调用
    Logger::Global().Debug("Application started");
    Logger::Global().Info("This is an info message");
    Logger::Global().Error("Something went wrong!");

    // 格式化输出
    Logger::Global().DebugFmt("User %s logged in from IP %s", "admin", "192.168.1.1");

    // 实例化调用
    Logger networkLogger("Network");
    networkLogger.SetLevel(Logger::Level::Trace);
    networkLogger.SetTarget(Logger::Target::Console);

    networkLogger.Trace("Network connection established");
    networkLogger.Info("Received 1024 bytes of data");
    networkLogger.WarningFmt("Slow connection detected: %.2f kbps", 128.5);

    // 另一个实例
    Logger dbLogger("Database", Logger::Level::Warning, Logger::Target::File, "f:\\db.log");
    dbLogger.Info("This won't be logged due to level setting");
    dbLogger.Warning("Database query took too long");

    return 0;
}