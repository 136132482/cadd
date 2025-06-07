#include "ThreadExecutor.h"
#include <iostream>
#include <string>
#include <utility>
#include "../fileIo/FileTransferUtility.h"

// 示例函数1: 无返回值
void printMessage(const std::string& msg) {
    std::cout << "Thread " << std::this_thread::get_id()
              << ": " << msg << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 示例函数2: 有返回值
int computeSquare(int x) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return x * x;
}

int main() {
    try {
        // 1. 创建线程执行器 (默认异步模式)
        ThreadExecutor executor;
        FileTransferUtility ft;

        // 2. 执行单个任务
        auto future1 = executor.execute(computeSquare, 5);
        std::cout << "Square result: " << future1.get() << std::endl;


        executor.execute([&ft](const std::string& url, const std::string& path, bool resume,
                                    std::function<void(const std::string&, uint64_t, uint64_t, double)> progress) {
                                  return ft.downloadFile(url, path, resume, std::move(progress));
                              },
                              "https://3g79q59242.imdo.co/test_voice/陈文林.docx",
                              "F:\\xxx.docx",
                              true,
                              [](const std::string& file, uint64_t done, uint64_t total, double speed) {
                                  std::cout << file << " - Progress: " << (done*100/total)
                                            << "%, Speed: " << speed << " KB/s\n";
                              });

        // 1. 准备下载任务列表
        std::vector<std::tuple<std::string, std::string, bool>> downloads = {
                {"https://3g79q59242.imdo.co/pic/1.jpeg", "f:/images/file1.jpg", true},
                {"https://3g79q59242.imdo.co/pic/2.jpeg", "f:/images/file2.jpg", true},
                {"https://3g79q59242.imdo.co/pic/3.jpeg", "f:/images/file3.jpg", true},
                {"https://3g79q59242.imdo.co/pic/4.jpeg", "f:/images/file4.jpg", true},
                {"https://3g79q59242.imdo.co/pic/5.jpeg", "f:/images/file5.jpg", true},
                {"https://3g79q59242.imdo.co/pic/1.jpeg", "f:/images/file1.jpg", true},
                {"https://3g79q59242.imdo.co/pic/2.jpeg", "f:/images/file2.jpg", true},
                {"https://3g79q59242.imdo.co/pic/3.jpeg", "f:/images/file3.jpg", true},
                {"https://3g79q59242.imdo.co/pic/4.jpeg", "f:/images/file4.jpg", true},
                {"https://3g79q59242.imdo.co/pic/5.jpeg", "f:/images/file5.jpg", true}
        };

        // 2. 定义处理函数（参数必须严格匹配tuple内容）
        auto downloadHandler = [&ft](const std::string& url,
                                     const std::string& path,
                                     bool resume) {
            // 创建符合ProgressCallback签名的lambda
            FileTransferUtility::ProgressCallback progress =
                    [path](const std::string&, uint64_t d, uint64_t t, double s) {
                        std::cout << path << " - " << (d*100/t) << "% (" << s << " KB/s)\n";
                    };

            return ft.downloadFile(url, path, resume, progress);
        };

       // 3. 执行批量下载
        executor.executeBatch(downloads, downloadHandler,
                              [](size_t done, size_t total) {
                                  std::cout << "单个任务整体进度: " << done << "/" << total << "\n";
                              });


// 2. 将pair转换为tuple并添加resume参数
//        std::vector<std::tuple<std::string, std::string, bool>> batchTasks;
//        for (const auto& [url, path, resume] : downloads) {
//            batchTasks.emplace_back(url, path, resume); // 全部启用断点续传
//        }


        // 3. 定义批处理执行函数
        auto batchExecutor = [&ft](const std::string& url,
                                   const std::string& path,
                                   bool resume) {
            // 创建进度回调
            auto progress = [path](const std::string&, uint64_t d, uint64_t t, double s) {
                std::cout << path << " - " << (d*100/t) << "% (" << s << " KB/s)\n";
            };

            // 创建状态回调
            auto stateCb = [path](const std::string&, FileTransferUtility::TransferState state) {
                std::cout << path << " state: " << static_cast<int>(state) << std::endl;
            };

            // 调用batchDownload（注意这里需要调整batchDownload接口）
            std::vector<std::pair<std::string, std::string>> singleTask{{url, path}};
            return ft.batchDownload(singleTask, resume, progress, stateCb);
        };

//        TaskGrouper<std::string, std::string, bool> grouper(downloads, 3);
//
//        for (const auto& group : grouper.getGroups()) {
//            // 4. 执行多线程批量下载
//            executor.executeBatch(group, batchExecutor,
//                                  [](size_t done, size_t total) {
//                                      std::cout << "整体进度: " << done << "/" << total << " 个文件\n";
//                                  });
//
//        }
//            ThreadExecutor::Config syncConfig(ThreadExecutor::ExecutionMode::SYNC,5);
//            executor.setConfig(syncConfig);
            // 4. 执行多线程批量下载
            executor.executeBatch(downloads, batchExecutor,
                                  [](size_t done, size_t total) {
                                      std::cout << "批量整体进度: " << done << "/" << total << " 个文件\n";
                                  });

        // 3. 批量执行任务
        std::vector<std::tuple<std::string>> messages = {
                std::make_tuple("Task 1"),
                std::make_tuple("Task 2"),
                std::make_tuple("Task 3"),
                std::make_tuple("Task 4")
        };

        executor.executeBatch(messages, printMessage,
                              [](size_t current, size_t total) {
                                  std::cout << "Progress: " << current << "/" << total << std::endl;
                              }
        );

        // 4. 切换为同步模式
        ThreadExecutor::Config syncConfig1(ThreadExecutor::ExecutionMode::SYNC);
        executor.setConfig(syncConfig1);

        // 5. 同步执行任务
        std::vector<std::tuple<int>> numbers = {
                std::make_tuple(1),
                std::make_tuple(2),
                std::make_tuple(3)
        };

        std::vector<std::future<int>> futures;
        for (auto& num : numbers) {
            futures.push_back(executor.execute(computeSquare, std::get<0>(num)));
        }

        for (auto& future : futures) {
            std::cout << "Result: " << future.get() << std::endl;
        }

        // 6. 显式等待所有任务完成
        executor.waitAll();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}