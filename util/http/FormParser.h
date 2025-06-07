#pragma once
#include <string>
#include <map>
#include <string_view>

class FormParser {
public:
    static std::map<std::string, std::string> Parse(const drogon::HttpRequestPtr& req) {
        std::map<std::string, std::string> result;
        std::string_view body = req->getBody(); // 使用 string_view
        std::string_view content_type = req->getHeader("Content-Type");

        // 提取 boundary
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos == std::string_view::npos) return {};
        std::string boundary = "--" + std::string(content_type.substr(boundary_pos + 9));

        // 解析逻辑
        size_t pos = 0;
        while ((pos = body.find(boundary, pos)) != std::string_view::npos) {
            pos += boundary.length();
            if (body.substr(pos, 2) == "--") break;

            size_t name_start = body.find("name=\"", pos) + 6;
            size_t name_end = body.find("\"", name_start);
            std::string name(body.substr(name_start, name_end - name_start)); // 显式构造 string

            size_t value_start = body.find("\r\n\r\n", name_end) + 4;
            size_t value_end = body.find(boundary, value_start);
            result[name] = std::string(body.substr(value_start, value_end - value_start - 2));

            pos = value_end;
        }
        return result;
    }
};