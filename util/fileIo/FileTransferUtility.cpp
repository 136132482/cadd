#include "FileTransferUtility.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

namespace fs = std::filesystem;

FileTransferUtility::FileTransferUtility() {}

FileTransferUtility::~FileTransferUtility() {}

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
    if(!exists(localPath)) {
        Logger::Global().Error("Local file does not exist: " + localPath);
        return false;
    }

    TransferTask task;
    task.identifier = localPath;
    task.sourcePath = localPath;
    task.targetPath = remoteUrl;
    task.isUpload = true;
    task.resumeEnabled = resume && isResumeSupported(remoteUrl);
    task.totalBytes = getFileSize(localPath);
    task.transferredBytes = 0;
    task.state = TransferState::RUNNING;
    task.paused = false;
    task.cancelled = false;
    task.progressCallback = std::move(progressCb);

    return doUpload(task);
}

bool FileTransferUtility::downloadFile(const string& remoteUrl, const string& localPath,
                                       bool resume, ProgressCallback progressCb) {
    TransferTask task;
    task.identifier = remoteUrl;
    task.sourcePath = remoteUrl;
    task.targetPath = localPath;
    task.isUpload = false;
    task.resumeEnabled = resume && isResumeSupported(remoteUrl);
    task.totalBytes = getRemoteFileSize(remoteUrl);
    task.transferredBytes = 0;
    task.state = TransferState::RUNNING;
    task.paused = false;
    task.cancelled = false;
    task.progressCallback = std::move(progressCb);

    return doDownload(task);
}

void FileTransferUtility::batchUpload(const vector<pair<string, string>>& filePairs,
                                      bool resume,
                                      ProgressCallback progressCb,
                                      StateCallback stateCb) {
    for(const auto& pair : filePairs) {
        TransferTask task;
        task.identifier = pair.first;
        task.sourcePath = pair.first;
        task.targetPath = pair.second;
        task.isUpload = true;
        task.resumeEnabled = resume && isResumeSupported(pair.second);
        task.totalBytes = getFileSize(pair.first);
        task.transferredBytes = 0;
        task.state = TransferState::RUNNING;
        task.paused = false;
        task.cancelled = false;
        task.progressCallback = progressCb;
        task.stateCallback = stateCb;

        if(stateCb) {
            stateCb(pair.first, TransferState::RUNNING);
        }

        bool success = doUpload(task);

        if(stateCb) {
            stateCb(pair.first, success ? TransferState::COMPLETED : TransferState::FAILED);
        }
    }
}

void FileTransferUtility::batchDownload(const vector<pair<string, string>>& urlPairs,
                                        bool resume,
                                        ProgressCallback progressCb,
                                        StateCallback stateCb) {
    for(const auto& pair : urlPairs) {
        TransferTask task;
        task.identifier = pair.first;
        task.sourcePath = pair.first;
        task.targetPath = pair.second;
        task.isUpload = false;
        task.resumeEnabled = resume && isResumeSupported(pair.first);
        task.totalBytes = getRemoteFileSize(pair.first);
        task.transferredBytes = 0;
        task.state = TransferState::RUNNING;
        task.paused = false;
        task.cancelled = false;
        task.progressCallback = progressCb;
        task.stateCallback = stateCb;

        if(stateCb) {
            stateCb(pair.first, TransferState::RUNNING);
        }

        bool success = doDownload(task);

        if(stateCb) {
            stateCb(pair.first, success ? TransferState::COMPLETED : TransferState::FAILED);
        }
    }
}

bool FileTransferUtility::doUpload(TransferTask& task) {
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
            Logger::Global().Debug("Resuming upload from position: " + to_string(resumePosition));
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
        Logger::Global().Error("Upload failed (" + string(curl_easy_strerror(res)) + "): " + task.sourcePath);
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
        Logger::Global().Error("Failed to initialize CURL for download: " + task.sourcePath);
        return false;
    }

    Logger::Global().Info("Starting download: " + task.sourcePath + " -> " + task.targetPath);

    // 检查断点续传
    uint64_t resumePosition = 0;
    string tempFilePath = generateTempFilePath(task.targetPath);

    if (task.resumeEnabled && fs::exists(tempFilePath)) {
        try {
            resumePosition = fs::file_size(tempFilePath);
            Logger::Global().Debug("Found partial file, resuming from: " + to_string(resumePosition));
        } catch (const fs::filesystem_error& e) {
            Logger::Global().Error("Failed to get partial file size: " + string(e.what()));
        }
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

    FILE* fp = fopen(tempFilePath.c_str(), resumePosition > 0 ? "ab" : "wb");
    if (!fp) {
        Logger::Global().Error("Failed to open temp file: " + tempFilePath + " - " + strerror(errno));
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
        Logger::Global().Error("Download failed (" + string(curl_easy_strerror(res)) + "): " + task.sourcePath);
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
    try {
        fs::path p(originalPath);
        if (p.parent_path().empty()) {
            fs::path tempDir = fs::temp_directory_path();
            return (tempDir / (p.stem().string() + "_part" + p.extension().string())).string();
        }
        fs::path tempPath = p.parent_path() / (p.stem().string() + "_part" + p.extension().string());
        return tempPath.string();
    } catch (const exception& e) {
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
    return url.find("http://") == 0 || url.find("https://") == 0 || url.find("ftp://") == 0;
}