#include "FileTransferUtility.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>  // for std::transform
#include <cctype>     // for ::tolower
#include <filesystem>

using namespace std;
using namespace std::filesystem;

namespace fs = std::filesystem;

// ThreadPool实现
FileTransferUtility::ThreadPool::ThreadPool(size_t threads) : stop(false) {

    for(size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                function<void()> task;
                {
                    unique_lock<mutex> lock(queueMutex);
                    condition.wait(lock, [this]{ return stop || !tasks.empty(); });
                    if(stop && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

template<class F, class... Args>
auto FileTransferUtility::ThreadPool::enqueue(F&& f, Args&&... args)
-> future<typename result_of<F(Args...)>::type> {
    using return_type = typename result_of<F(Args...)>::type;

    auto task = make_shared<packaged_task<return_type()>>(
            bind(std::forward<F>(f), forward<Args>(args)...)
    );

    future<return_type> res = task->get_future();
    {
        lock_guard<mutex> lock(queueMutex);
        if(stop) throw runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

FileTransferUtility::ThreadPool::~ThreadPool() {
    {
        lock_guard<mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for(thread &worker: workers)
        worker.join();
}

// FileTransferUtility实现
FileTransferUtility::FileTransferUtility()
        : threadPool(make_unique<ThreadPool>(thread::hardware_concurrency())) {}

FileTransferUtility::~FileTransferUtility() {
    cancelAllTransfers();
}

FileTransferUtility::FileType FileTransferUtility::getFileType(const string& filePath) {
    string ext = getFileExtension(filePath);
    auto it = FILE_TYPE_MAP.find(ext);
    return it != FILE_TYPE_MAP.end() ? it->second : FileType::UNKNOWN;
}

string FileTransferUtility::getFileExtension(const string& filePath) {
    path p(filePath);
    string ext = p.extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

uint64_t FileTransferUtility::getFileSize(const string& filePath) {
    error_code ec;
    uintmax_t size = file_size(filePath, ec);
    return ec ? 0 : size;
}

uint64_t FileTransferUtility::getRemoteFileSize(const string& url) {
    CURL* curl = curl_easy_init();
    if(!curl) return 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if(curl_easy_perform(curl) != CURLE_OK) {
        curl_easy_cleanup(curl);
        return 0;
    }

    curl_off_t size;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);

    curl_easy_cleanup(curl);
    return size > 0 ? size : 0;
}

bool FileTransferUtility::uploadFile(const string& localPath, const string& remoteUrl,
                                     bool resume, ProgressCallback progressCb) {
    if(!exists(localPath)) return false;

    auto task = make_shared<TransferTask>();
    task->identifier = localPath;
    task->sourcePath = localPath;
    task->targetPath = remoteUrl;
    task->isUpload = true;
    task->resumeEnabled = resume && isResumeSupported(remoteUrl);
    task->totalBytes = getFileSize(localPath);
    task->transferredBytes = 0;
    task->state = TransferState::READY;
    task->paused = false;
    task->cancelled = false;
    task->progressCallback = std::move(progressCb);

    {
        lock_guard<mutex> lock(tasksMutex);
        activeTasks[localPath] = task;
    }

    executeTransferTask(task);
    return true;
}

bool FileTransferUtility::downloadFile(const string& remoteUrl, const string& localPath,
                                       bool resume, ProgressCallback progressCb) {
    auto task = make_shared<TransferTask>();
    task->identifier = remoteUrl;
    task->sourcePath = remoteUrl;
    task->targetPath = localPath;
    task->isUpload = false;
    task->resumeEnabled = resume && isResumeSupported(remoteUrl);
    task->totalBytes = getRemoteFileSize(remoteUrl);
    task->transferredBytes = 0;
    task->state = TransferState::READY;
    task->paused = false;
    task->cancelled = false;
    task->progressCallback = std::move(progressCb);

    {
        lock_guard<mutex> lock(tasksMutex);
        activeTasks[remoteUrl] = task;
    }

    executeTransferTask(task);
    return true;
}

void FileTransferUtility::batchUpload(const vector<pair<string, string>>& filePairs,
                                      int maxThreads, bool resume,
                                      ProgressCallback progressCb, StateCallback stateCb) {
    auto pool = make_unique<ThreadPool>(maxThreads);
    vector<future<bool>> futures;

    for(const auto& pair : filePairs) {
        futures.emplace_back(pool->enqueue([this, pair, resume, progressCb, stateCb]() {
            auto task = make_shared<TransferTask>();
            task->identifier = pair.first;
            task->sourcePath = pair.first;
            task->targetPath = pair.second;
            task->isUpload = true;
            task->resumeEnabled = resume && isResumeSupported(pair.second);
            task->totalBytes = getFileSize(pair.first);
            task->transferredBytes = 0;
            task->state = TransferState::READY;
            task->paused = false;
            task->cancelled = false;
            task->progressCallback = progressCb;

            if(stateCb) {
                task->stateCallback = [stateCb, identifier = task->identifier](const string&, TransferState state) {
                    stateCb(identifier, state);
                };
            }

            {
                lock_guard<mutex> lock(tasksMutex);
                activeTasks[pair.first] = task;
            }

            executeTransferTask(task);
            return task->state == TransferState::COMPLETED;
        }));
    }

    // 等待所有任务完成
    for(auto& f : futures) {
        f.get();
    }
}

void FileTransferUtility::batchDownload(const vector<pair<string, string>>& urlPairs,
                                        int maxThreads, bool resume,
                                        ProgressCallback progressCb, StateCallback stateCb) {
    auto pool = make_unique<ThreadPool>(maxThreads);
    vector<future<bool>> futures;

    for(const auto& pair : urlPairs) {
        futures.emplace_back(pool->enqueue([this, pair, resume, progressCb, stateCb]() {
            auto task = make_shared<TransferTask>();
            task->identifier = pair.first;
            task->sourcePath = pair.first;
            task->targetPath = pair.second;
            task->isUpload = false;
            task->resumeEnabled = resume && isResumeSupported(pair.first);
            task->totalBytes = getRemoteFileSize(pair.first);
            task->transferredBytes = 0;
            task->state = TransferState::READY;
            task->paused = false;
            task->cancelled = false;
            task->progressCallback = progressCb;

            if(stateCb) {
                task->stateCallback = [stateCb, identifier = task->identifier](const string&, TransferState state) {
                    stateCb(identifier, state);
                };
            }

            {
                lock_guard<mutex> lock(tasksMutex);
                activeTasks[pair.first] = task;
            }

            executeTransferTask(task);
            return task->state == TransferState::COMPLETED;
        }));
    }

    // 等待所有任务完成
    for(auto& f : futures) {
        f.get();
    }
}

void FileTransferUtility::pauseTransfer(const string& fileIdentifier) {
    lock_guard<mutex> lock(tasksMutex);
    auto it = activeTasks.find(fileIdentifier);
    if(it != activeTasks.end()) {
        it->second->paused = true;
        it->second->state = TransferState::PAUSED;

        if(it->second->stateCallback) {
            it->second->stateCallback(it->second->identifier, TransferState::PAUSED);
        }
    }
}

void FileTransferUtility::resumeTransfer(const string& fileIdentifier) {
    lock_guard<mutex> lock(tasksMutex);
    auto it = activeTasks.find(fileIdentifier);
    if(it != activeTasks.end() && it->second->state == TransferState::PAUSED) {
        it->second->paused = false;
        it->second->state = TransferState::READY;

        if(it->second->stateCallback) {
            it->second->stateCallback(it->second->identifier, TransferState::READY);
        }

        executeTransferTask(it->second);
    }
}

void FileTransferUtility::cancelTransfer(const string& fileIdentifier) {
    lock_guard<mutex> lock(tasksMutex);
    auto it = activeTasks.find(fileIdentifier);
    if(it != activeTasks.end()) {
        it->second->cancelled = true;
        it->second->state = TransferState::CANCELLED;

        if(it->second->stateCallback) {
            it->second->stateCallback(it->second->identifier, TransferState::CANCELLED);
        }

        activeTasks.erase(it);
    }
}

void FileTransferUtility::cancelAllTransfers() {
    lock_guard<mutex> lock(tasksMutex);
    for(auto& pair : activeTasks) {
        pair.second->cancelled = true;
        pair.second->state = TransferState::CANCELLED;

        if(pair.second->stateCallback) {
            pair.second->stateCallback(pair.second->identifier, TransferState::CANCELLED);
        }
    }
    activeTasks.clear();
}

FileTransferUtility::TransferState FileTransferUtility::getTransferState(const string& fileIdentifier) const {
    lock_guard<mutex> lock(tasksMutex);
    auto it = activeTasks.find(fileIdentifier);
    return it != activeTasks.end() ? it->second->state.load() : TransferState::FAILED;
}

map<string, FileTransferUtility::TransferState> FileTransferUtility::getAllTransferStates() const {
    map<string, TransferState> states;
    lock_guard<mutex> lock(tasksMutex);
    for(const auto& pair : activeTasks) {
        states[pair.first] = pair.second->state.load();
    }
    return states;
}

void FileTransferUtility::executeTransferTask(shared_ptr<TransferTask> task) {
    task->workerThread = make_shared<thread>([this, task]() {
        task->state = TransferState::RUNNING;

        if(task->stateCallback) {
            task->stateCallback(task->identifier, TransferState::RUNNING);
        }

        bool success = false;
        if(task->isUpload) {
            success = doUpload(*task);
        } else {
            success = doDownload(*task);
        }

        if(task->cancelled) {
            task->state = TransferState::CANCELLED;
        } else if(success) {
            task->state = TransferState::COMPLETED;
        } else {
            task->state = TransferState::FAILED;
        }

        if(task->stateCallback) {
            task->stateCallback(task->identifier, task->state.load());
        }

        // 从活动任务中移除
        if(task->state != TransferState::PAUSED) {
            lock_guard<mutex> lock(tasksMutex);
            activeTasks.erase(task->identifier);
        }
    });
}

bool FileTransferUtility::doUpload(TransferTask& task) {
    // 实际实现需要根据上传API调整
    // 这里使用CURL模拟上传过程

    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::Global().Error("Failed to initialize CURL for upload: " + task.sourcePath);
        return false;
    }

    Logger::Global().Info("Starting upload: " + task.sourcePath + " -> " + task.targetPath);

    // 检查断点续传
    uint64_t resumePosition = 0;
    if (task.resumeEnabled) {
        string resumeInfoPath = generateResumeInfoPath(task.sourcePath);
        if (loadResumeInfo(resumeInfoPath, resumePosition)) {
            Logger::Global().Debug("Resuming upload from position: " + std::to_string(resumePosition));
        }
    }

    FILE* fp = fopen(task.sourcePath.c_str(), "rb");
    if (!fp) {
        Logger::Global().Error("Failed to open source file: " + task.sourcePath + " - " + strerror(errno));
        curl_easy_cleanup(curl);
        return false;
    }

    if(resumePosition > 0) {
        fseek(fp, resumePosition, SEEK_SET);
        task.transferredBytes = resumePosition;
    }

    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_URL, task.targetPath.c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readDataCallback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(task.totalBytes - resumePosition));

    // 进度回调
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &task);

    // 执行上传
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        Logger::Global().Error("Upload failed (" + std::string(curl_easy_strerror(res)) + "): " + task.sourcePath);
    } else {
        Logger::Global().Info("Upload completed: " + task.sourcePath);
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    // 清理断点续传信息
    if(res == CURLE_OK && task.resumeEnabled) {
        removeResumeInfo(generateResumeInfoPath(task.sourcePath));
    }

    return res == CURLE_OK && !task.cancelled && !task.paused;
}

bool FileTransferUtility::doDownload(TransferTask& task) {

    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::Global().Error("Failed to initialize CURL for resume check");
        return false;
    }
    Logger::Global().Info("Starting download: " + task.sourcePath + " -> " + task.targetPath);

    // 检查是否需要断点续传
    uint64_t resumePosition = 0;
    string tempFilePath = generateTempFilePath(task.targetPath);

    // 检查生成的临时文件路径是否为空
    if (tempFilePath.empty()) {
        Logger::Global().Error("Generated temporary file path is empty!");
        curl_easy_cleanup(curl);
        return false;
    }

    // 打印临时文件路径以确认
    Logger::Global().Info("Generated temporary file path: " + tempFilePath);

    // 检查路径是否合法
    try {
        fs::path tempPath(tempFilePath);
        if (tempPath.empty()) {
            throw std::runtime_error("Generated path is empty!");
        }
        fs::path tempDir = tempPath.parent_path();
        if (tempDir.empty()) {
            throw std::runtime_error("Parent directory is empty!");
        }
    } catch (const std::exception& e) {
        Logger::Global().Error("Invalid temp file path: " + tempFilePath + " | Error: " + e.what());
        curl_easy_cleanup(curl);
        return false;
    }

    // 确保目录存在
    fs::path tempDir = fs::path(tempFilePath).parent_path();
    if (!fs::exists(tempDir)) {
        try {
            fs::create_directories(tempDir);
            Logger::Global().Info("Created directory: " + tempDir.string());
        } catch (const fs::filesystem_error& e) {
            Logger::Global().Error("Failed to create directory: " + string(e.what()));
            curl_easy_cleanup(curl);
            return false;
        }
    }

    if (task.resumeEnabled && fs::exists(tempFilePath)) {
        try {
            resumePosition = fs::file_size(tempFilePath);
            Logger::Global().Debug("Found partial file, resuming from: " + std::to_string(resumePosition));
        } catch (const fs::filesystem_error& e) {
            Logger::Global().Error("Failed to get partial file size: " + string(e.what()));
        }
    }
    Logger::Global().Info("Temporary file path: " + tempFilePath);

    // 打开文件并增加详细错误检查
    FILE* fp = fopen(tempFilePath.c_str(), resumePosition > 0 ? "ab" : "wb");
    if (!fp) {
        DWORD error = GetLastError();
        LPSTR errorMsg = nullptr;
        FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                nullptr, error, 0, (LPSTR)&errorMsg, 0, nullptr
        );

        Logger::Global().Error("Failed to open temp file: " + tempFilePath +
                               " | Error: " + (errorMsg ? errorMsg : "Unknown") +
                               " | GetLastError: " + std::to_string(error));

        LocalFree(errorMsg);
        curl_easy_cleanup(curl);
        return false;
    }

    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_URL, task.sourcePath.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataCallback);

    if (resumePosition > 0) {
        string range = to_string(resumePosition) + "-";
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    }

    // 进度回调
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &task);

    // 执行下载
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        Logger::Global().Error("Download failed (" + std::string(curl_easy_strerror(res)) + "): " + task.sourcePath);
    } else {
        Logger::Global().Info("Download completed: " + task.sourcePath);
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    // 重命名临时文件
    if (res == CURLE_OK && !task.cancelled && !task.paused) {
        try {
            if (fs::exists(task.targetPath)) {
                fs::remove(task.targetPath);
            }
            fs::rename(tempFilePath, task.targetPath);
            return true;
        } catch (const fs::filesystem_error& e) {
            Logger::Global().Error("Failed to rename file: " + string(e.what()));
            return false;
        }
    }

    // 保存断点续传信息
    if (task.resumeEnabled && (task.paused || task.cancelled)) {
        string resumeInfoPath = generateResumeInfoPath(task.targetPath);
        saveResumeInfo(resumeInfoPath, task.transferredBytes);
    }

    return res == CURLE_OK && !task.cancelled && !task.paused;
}

size_t FileTransferUtility::writeDataCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    return fwrite(ptr, size, nmemb, (FILE*)userdata);
}

size_t FileTransferUtility::readDataCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    return fread(ptr, size, nmemb, (FILE*)userdata);
}

int FileTransferUtility::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                          curl_off_t ultotal, curl_off_t ulnow) {
    auto task = static_cast<TransferTask*>(clientp);

    if(task->cancelled) return 1;
    if(task->paused) return 1;

    // 计算传输速度
    static auto lastTime = chrono::steady_clock::now();
    static uint64_t lastBytes = 0;

    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastTime).count();

    if(elapsed >= 1000) { // 每秒更新一次速度
        uint64_t currentBytes = task->isUpload ? ulnow : dlnow;
        double speed = (currentBytes - lastBytes) / (elapsed / 1000.0) / 1024.0; // KB/s

        if(task->progressCallback) {
            uint64_t total = task->isUpload ? ultotal : dltotal;
            uint64_t transferred = task->transferredBytes + (task->isUpload ? ulnow : dlnow);
            task->progressCallback(task->identifier, transferred, task->totalBytes, speed);
        }

        lastTime = now;
        lastBytes = currentBytes;
    }

    return 0;
}

string FileTransferUtility::generateTempFilePath(const string& originalPath) {
    if (originalPath.empty()) {
        Logger::Global().Error("Original path is empty!");
        return "";
    }

    try {
        fs::path p(originalPath);
        if (p.empty()) {
            throw std::runtime_error("Constructed path is empty!");
        }

        // 如果 originalPath 不包含父目录，则使用默认临时目录
        if (p.parent_path().empty()) {
            // 例如，使用系统临时目录
            fs::path tempDir = fs::temp_directory_path();
            return (tempDir / (p.stem().string() + "_part" + p.extension().string())).string();
        }

        // 使用 "_part" 代替 ".part"
        fs::path tempPath = p.parent_path() / (p.stem().string() + "_part" + p.extension().string());
        Logger::Global().Info("Generated temporary file path: " + tempPath.string());
        return tempPath.string();
    } catch (const std::exception& e) {
        Logger::Global().Error("Failed to generate temp file path: " + string(e.what()));
        return "";
    }
}

string FileTransferUtility::generateResumeInfoPath(const string& filePath) {
    return filePath + ".resume";
}

bool FileTransferUtility::loadResumeInfo(const string& infoPath, uint64_t& resumePosition) {
    ifstream in(infoPath, ios::binary);
    if(!in) return false;

    in.read(reinterpret_cast<char*>(&resumePosition), sizeof(resumePosition));
    return in.good();
}

bool FileTransferUtility::saveResumeInfo(const string& infoPath, uint64_t resumePosition) {
    ofstream out(infoPath, ios::binary);
    if(!out) return false;

    out.write(reinterpret_cast<const char*>(&resumePosition), sizeof(resumePosition));
    return out.good();
}

bool FileTransferUtility::removeResumeInfo(const string& infoPath) {
    error_code ec;
    return remove(infoPath, ec);
}

bool FileTransferUtility::isResumeSupported(const string& url) {
    Logger::Global().Debug("Checking resume support for: " + url);

    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::Global().Debug("Failed to initialize CURL for resume check");
        return false;
    }

    // 简单检查协议是否支持断点续传
    return url.find("http://") == 0 || url.find("https://") == 0 || url.find("ftp://") == 0;
}