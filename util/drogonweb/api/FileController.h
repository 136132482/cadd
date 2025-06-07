#pragma once
#include <drogon/HttpController.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

class FileController : public drogon::HttpController<FileController, false> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(FileController::upload, "/api/upload", drogon::Post);
    METHOD_LIST_END

    struct UploadedFile {
        std::string filename;
        std::string content;
        size_t size;
        std::string upload_time;
    };

    // 获取项目根目录
    static std::string getProjectRoot() {
#ifdef CMAKE_SOURCE_DIR
        return CMAKE_SOURCE_DIR;
#else
        std::filesystem::path exe_path;
        #ifdef _WIN32
        char buf[MAX_PATH];
        GetModuleFileNameA(NULL, buf, MAX_PATH);
        exe_path = std::filesystem::path(buf);
        #else
        char buf[1024];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
        if (len != -1) {
            buf[len] = '\0';
            exe_path = std::filesystem::path(buf);
        }
        #endif

        if (!exe_path.empty()) {
            auto project_root = exe_path.parent_path().parent_path().parent_path();
            if (std::filesystem::exists(project_root / "CMakeLists.txt")) {
                return project_root.string();
            }
        }
        return std::filesystem::current_path().string();
#endif
    }

    // 获取当前时间字符串
    static std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // 打印上传日志到控制台
    static void printUploadLog(const UploadedFile& file,
                               const std::string& save_path,
                               const drogon::HttpRequestPtr& req) {
        std::string clientIP = req->getHeader("X-Forwarded-For");
        if(clientIP.empty()) {
            clientIP = req->getPeerAddr().toIp();
        }

        std::cout << "[" << file.upload_time << "] "
                  << "File: \"" << file.filename << "\" | "
                  << "Size: " << file.size << " bytes | "
                  << "Path: \"" << save_path << "\" | "
                  << "IP: " << clientIP << std::endl;
    }

    static void upload(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        UploadedFile uploaded_file;
        uploaded_file.upload_time = getCurrentTime();

        try {
            if (req->contentType() != drogon::CT_MULTIPART_FORM_DATA) {
                throw std::runtime_error("Content-Type must be multipart/form-data");
            }

            auto files = parseMultipart(req);
            if (files.empty()) {
                throw std::runtime_error("No files uploaded");
            }

            std::string project_root = getProjectRoot();
            std::string upload_dir = project_root + "/uploads";

            if (!std::filesystem::exists(upload_dir)) {
                if (!std::filesystem::create_directories(upload_dir)) {
                    throw std::runtime_error("Failed to create upload directory");
                }
            }

            auto& file = files[0];
            uploaded_file.filename = file.filename;
            uploaded_file.size = file.content.size();

            std::filesystem::path save_path = std::filesystem::path(upload_dir) /
                                              std::filesystem::path(file.filename).filename();

            std::ofstream out(save_path, std::ios::binary);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open file for writing: " + save_path.string());
            }

            out.write(file.content.data(), file.content.size());
            out.close();

            // 改为打印日志到控制台
            printUploadLog(uploaded_file, save_path.string(), req);

            Json::Value ret;
            ret["status"] = "success";
            ret["filename"] = file.filename;
            ret["size"] = static_cast<Json::UInt64>(file.content.size());
            ret["saved_path"] = save_path.string();
            ret["upload_time"] = uploaded_file.upload_time;
            callback(drogon::HttpResponse::newHttpJsonResponse(ret));

        } catch (const std::exception& e) {
            // 打印错误日志到控制台
            std::cerr << "[" << uploaded_file.upload_time << "] "
                      << "File: " << uploaded_file.filename << " | "
                      << "Error: " << e.what() << " | "
                      << "IP: " << req->getPeerAddr().toIp() << std::endl;

            Json::Value errorResponse;
            errorResponse["status"] = "error";
            errorResponse["message"] = e.what();
            errorResponse["upload_time"] = uploaded_file.upload_time;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(errorResponse);
            resp->setStatusCode(drogon::k500InternalServerError);
            callback(resp);
        }
    }

private:
    static std::vector<UploadedFile> parseMultipart(const drogon::HttpRequestPtr& req) {
        std::vector<UploadedFile> files;
        std::string_view body = req->getBody();
        std::string_view contentType = req->getHeader("Content-Type");

        size_t boundary_pos = contentType.find("boundary=");
        if (boundary_pos == std::string_view::npos) return files;

        std::string boundary = "--" + std::string(contentType.substr(boundary_pos + 9));
        size_t pos = 0;

        while ((pos = body.find(boundary, pos)) != std::string_view::npos) {
            pos += boundary.length();
            if (pos + 2 > body.size() || body.substr(pos, 2) == "--") break;

            size_t name_pos = body.find("filename=\"", pos);
            if (name_pos == std::string_view::npos) continue;
            name_pos += 10;

            size_t name_end = body.find('"', name_pos);
            if (name_end == std::string_view::npos) continue;

            UploadedFile file;
            file.filename = std::string(body.substr(name_pos, name_end - name_pos));

            size_t header_end = body.find("\r\n\r\n", name_end);
            if (header_end == std::string_view::npos) continue;

            size_t content_start = header_end + 4;
            size_t content_end = body.find(boundary, content_start);
            if (content_end == std::string_view::npos) content_end = body.size();

            if (content_start + 2 <= content_end) {
                file.content = std::string(body.substr(
                        content_start,
                        content_end - content_start - 2
                ));
                files.push_back(std::move(file));
            }

            pos = content_end;
        }
        return files;
    }
};