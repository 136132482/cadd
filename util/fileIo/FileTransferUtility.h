#pragma once
#include "../Logger/Logger.h"
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <functional>
#include <future>
#include <queue>
#include <condition_variable>
#include <filesystem>
#include <curl/curl.h>
#include <iostream>

class FileTransferUtility {
public:
    // 文件类型枚举
    enum class FileType {
        UNKNOWN,
        IMAGE,
        VIDEO,
        TEXT,
        EXCEL,
        WORD,
        PPT,
        PDF,
        ARCHIVE
    };

    // 传输状态
    enum class TransferState {
        READY,
        RUNNING,
        PAUSED,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    // 进度回调 (文件名, 已传输字节, 总字节, 传输速度KB/s)
    using ProgressCallback = std::function<void(const std::string&, uint64_t, uint64_t, double)>;

    // 状态回调 (文件名, 状态)
    using StateCallback = std::function<void(const std::string&, TransferState)>;

    // 构造与析构
    FileTransferUtility();
    ~FileTransferUtility();

    // 单文件操作
    bool uploadFile(const std::string& localPath, const std::string& remoteUrl,
                    bool resume = false, ProgressCallback progressCb = nullptr);
    bool downloadFile(const std::string& remoteUrl, const std::string& localPath,
                      bool resume = false, ProgressCallback progressCb = nullptr);

    // 批量操作
    void batchUpload(const std::vector<std::pair<std::string, std::string>>& filePairs,
                     int maxThreads = 4, bool resume = false,
                     ProgressCallback progressCb = nullptr, StateCallback stateCb = nullptr);

    void batchDownload(const std::vector<std::pair<std::string, std::string>>& urlPairs,
                       int maxThreads = 4, bool resume = false,
                       ProgressCallback progressCb = nullptr, StateCallback stateCb = nullptr);

    // 控制操作
    void pauseTransfer(const std::string& fileIdentifier);
    void resumeTransfer(const std::string& fileIdentifier);
    void cancelTransfer(const std::string& fileIdentifier);
    void cancelAllTransfers();

    // 获取状态
    TransferState getTransferState(const std::string& fileIdentifier) const;
    std::map<std::string, TransferState> getAllTransferStates() const;

    // 工具方法
    static FileType getFileType(const std::string& filePath);
    static std::string getFileExtension(const std::string& filePath);
    static uint64_t getFileSize(const std::string& filePath);
    static uint64_t getRemoteFileSize(const std::string& url);

private:
    // 传输任务信息
    struct TransferTask {
        std::string identifier;  // 文件唯一标识(本地路径或URL)
        std::string sourcePath;  // 源路径(本地或远程)
        std::string targetPath;  // 目标路径(本地或远程)
        bool isUpload;           // 上传或下载
        bool resumeEnabled;      // 是否启用断点续传
        std::atomic<uint64_t> transferredBytes;
        std::atomic<uint64_t> totalBytes;
        std::atomic<TransferState> state;
        std::atomic<bool> paused;
        std::atomic<bool> cancelled;
        ProgressCallback progressCallback;
        StateCallback stateCallback;
        std::shared_ptr<std::thread> workerThread;
    };

    // 线程池
    class ThreadPool {
    public:
        explicit ThreadPool(size_t threads);
        ~ThreadPool();

        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable condition;
        std::atomic<bool> stop;
    };

    // 成员变量
    std::unique_ptr<ThreadPool> threadPool;
    mutable std::mutex tasksMutex;
    std::map<std::string, std::shared_ptr<TransferTask>> activeTasks;

    // 内部方法
    void executeTransferTask(std::shared_ptr<TransferTask> task);
    static bool doUpload(TransferTask& task);
    static bool doDownload(TransferTask& task);
    static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t readDataCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow);
    static std::string generateTempFilePath(const std::string& originalPath);
    static std::string generateResumeInfoPath(const std::string& filePath);
    static bool loadResumeInfo(const std::string& infoPath, uint64_t& resumePosition);
    static bool saveResumeInfo(const std::string& infoPath, uint64_t resumePosition);
    static bool removeResumeInfo(const std::string& infoPath);
    static bool isResumeSupported(const std::string& url);
};

// 文件类型映射
static const std::map<std::string, FileTransferUtility::FileType> FILE_TYPE_MAP = {
        {".jpg", FileTransferUtility::FileType::IMAGE},
        {".jpeg", FileTransferUtility::FileType::IMAGE},
        {".png", FileTransferUtility::FileType::IMAGE},
        {".gif", FileTransferUtility::FileType::IMAGE},
        {".bmp", FileTransferUtility::FileType::IMAGE},
        {".mp4", FileTransferUtility::FileType::VIDEO},
        {".avi", FileTransferUtility::FileType::VIDEO},
        {".mov", FileTransferUtility::FileType::VIDEO},
        {".mkv", FileTransferUtility::FileType::VIDEO},
        {".txt", FileTransferUtility::FileType::TEXT},
        {".csv", FileTransferUtility::FileType::TEXT},
        {".xml", FileTransferUtility::FileType::TEXT},
        {".xls", FileTransferUtility::FileType::EXCEL},
        {".xlsx", FileTransferUtility::FileType::EXCEL},
        {".doc", FileTransferUtility::FileType::WORD},
        {".docx", FileTransferUtility::FileType::WORD},
        {".ppt", FileTransferUtility::FileType::PPT},
        {".pptx", FileTransferUtility::FileType::PPT},
        {".pdf", FileTransferUtility::FileType::PDF},
        {".zip", FileTransferUtility::FileType::ARCHIVE},
        {".rar", FileTransferUtility::FileType::ARCHIVE},
        {".7z", FileTransferUtility::FileType::ARCHIVE}
};