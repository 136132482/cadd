#include "FileTransferUtility.h"
#include <iostream>
#include <vector>

int main() {
    FileTransferUtility ft;

    // 单个文件下载
    bool downloadResult = ft.downloadFile(
            "https://3g79q59242.imdo.co/test_voice/陈文林.docx",
            "F:\\xxx.docx",
            true, // 启用断点续传
            [](const std::string& file, uint64_t done, uint64_t total, double speed) {
                std::cout << file << " - Progress: " << (done*100/total)
                          << "%, Speed: " << speed << " KB/s\n";
            }
    );

    std::cout << "Download " << (downloadResult ? "succeeded" : "failed") << std::endl;

    // 单个文件上传
    bool uploadResult = ft.uploadFile(
            "F:\\xxx.docx",
            "https://3g79q59242.imdo.co/upload/www.docx",
            false,
            [](const std::string& file, uint64_t done, uint64_t total, double speed) {
                std::cout << file << " - Uploaded: " << done << "/" << total
                          << " bytes (" << speed << " KB/s)\n";
            }
    );

    std::cout << "Upload " << (uploadResult ? "succeeded" : "failed") << std::endl;

    // 批量下载
    std::vector<std::pair<std::string, std::string>> downloads = {
            {"https://3g79q59242.imdo.co/pic/1.jpeg", "f:/images/file1.jpg"},
            {"https://3g79q59242.imdo.co/pic/2.jpeg", "f:/images/file2.jpg"},
            {"https://3g79q59242.imdo.co/pic/3.jpeg", "f:/images/file3.jpg"}
    };

    ft.batchDownload(
            downloads,
            true, // 启用断点续传
            [](const std::string& file, uint64_t done, uint64_t total, double speed) {
                std::cout << "[Batch] " << file << " - " << (done*100/total) << "%\n";
            },
            [](const std::string& file, FileTransferUtility::TransferState state) {
                std::cout << "[Batch] " << file << " state changed to: "
                          << static_cast<int>(state) << std::endl;
            }
    );

    // 批量上传
    std::vector<std::pair<std::string, std::string>> uploads = {
            // 格式: {本地完整路径, 远程URL+唯一文件名}
            {"F:/images/file1.jpg", "https://3g79q59242.imdo.co/upload/pic1.jpg"},
            {"F:/images/file2.jpg", "https://3g79q59242.imdo.co/upload/pic2.jpg"},
            {"F:/images/file3.jpg", "https://3g79q59242.imdo.co/upload/pic3.jpg"}
    };

    ft.batchUpload(
            uploads,
            false,
            [](const std::string& file, uint64_t done, uint64_t total, double speed) {
                std::cout << "[Batch] " << file << " - " << (done*100/total) << "%\n";
            },
            [](const std::string& file, FileTransferUtility::TransferState state) {
                std::cout << "[Batch] " << file << " state changed to: "
                          << static_cast<int>(state) << std::endl;
            }
    );

    return 0;
}