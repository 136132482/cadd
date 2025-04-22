#include "FileTransferUtility.h"
#include <iostream>
#include <string>
#include <utility> // 包含 pair 的头文件
#include <vector>

using namespace std; // 或者使用 std:: 前缀





int main() {
    FileTransferUtility ft;
    Logger::Global().Info("Test message");          // 简化测试

    // 单个文件下载（带断点续传）
    ft.downloadFile(
            "https://3g79q59242.imdo.co/test_voice/111.docx",
            "F:\\test_voice\\xxx.docx",
            FALSE, // 启用断点续传
            [](const std::string& file, uint64_t done, uint64_t total, double speed) {
                std::cout << file << " - Progress: " << (done*100/total)
                     << "%, Speed: " << speed << " KB/s\n";
            }
    );

//    // 单个文件上传
//    ft.uploadFile(
//            "xxx.pdf",
//            "https://3g79q59242.imdo.co",
//            false,
//            [](const std::string& file, uint64_t done, uint64_t total, double speed) {
//                std::cout << file << " - Uploaded: " << done << "/" << total
//                     << " bytes (" << speed << " KB/s)\n";
//            }
//    );

//    // 批量下载
//    vector<pair<string, string>> downloads = {
//            {"http://example.com/file1.jpg", "images/file1.jpg"},
//            {"http://example.com/file2.mp4", "videos/file2.mp4"},
//            {"http://example.com/document.pdf", "docs/document.pdf"}
//    };
//
//    ft.batchDownload(
//            downloads,
//            3, // 最大3个并发下载
//            true, // 启用断点续传
//            [](const string& file, uint64_t done, uint64_t total, double speed) {
//                cout << "[Batch] " << file << " - " << (done*100/total) << "%\n";
//            },
//            [](const string& file, FileTransferUtility::TransferState state) {
//                cout << "[Batch] " << file << " state changed to: " << static_cast<int>(state) << endl;
//            }
//    );

    // 控制传输
    ft.pauseTransfer("https://3g79q59242.imdo.co/test_voice/111.docx");
//    ft.resumeTransfer("http://example.com/bigfile.zip");
//    ft.cancelTransfer("http://example.com/bigfile.zip");

    // 查询状态
    auto states = ft.getAllTransferStates();
    for(const auto& [file, state] : states) {
        cout << file << " is in state: " << static_cast<int>(state) << endl;
    }

    return 0;
}