#include "UserController.h"
#include "AuthFilter.h"
#include "LogFilter.h"
#include "FileController.h"

int main(int argc, char* argv[])
{
    // 1. 基础配置（直接链式调用）
    // 设置日志路径前检查目录
    std::string logPath = "./logs";
    if (!std::filesystem::exists(logPath)) {
        std::filesystem::create_directories(logPath);
    }
    drogon::app()
            .setLogPath(logPath)                      // 日志目录
            .setLogLevel(trantor::Logger::kInfo)       // 日志级别
            .setThreadNum(4)                           // 工作线程数（建议CPU核心数×2）
            .setDocumentRoot("./www")                  // 静态文件目录
            .setUploadPath("./uploads")                // 文件上传目录
            .setClientMaxBodySize(100000000)           // 最大请求体大小（100MB）
            .setIdleConnectionTimeout(60);             // 连接超时（秒）

    // 2. 监听端口配置（支持多端口监听）
    drogon::app().addListener("0.0.0.0", 8888);      // HTTP
    // drogon::app().addListener("0.0.0.0", 443)   // HTTPS（需配置SSL）
    //     .setSSLFiles("cert.pem", "key.pem");


    // 3. 注册控制器（自动加载路由）
    drogon::app()
            .registerController(std::make_shared<FileController>())
            .registerController(std::make_shared<UserController>());

    // 4. 高级功能（按需启用）
    // .enableSession(86400)                       // 会话超时（秒）
    // .enableGzip(true)                           // 启用压缩
    // .enableDrogonHttpClient(true)               // 启用内部HTTP客户端

    // 5. 运行前回调（初始化数据库等）
    drogon::app().registerBeginningAdvice([](){
        LOG_INFO << "正在初始化数据库连接...";
         // 注册插件
         SociDB::init();
    });

    // 6. 启动服务器（阻塞运行）
    LOG_INFO << "服务启动于 http://127.0.0.1:8888";
    drogon::app()
            .registerFilter(std::make_shared<AuthFilter>())
            .registerFilter(std::make_shared<LogFilter>())
    .run();

    return 0;
}