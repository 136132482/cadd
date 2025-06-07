
#pragma once
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <mutex>

class rapidJsonUtil {
public:
    // 解析JSON字符串
    static rapidjson::Document parse(const std::string& json_str) {
        rapidjson::Document doc;
        doc.Parse(json_str.c_str());
        if (doc.HasParseError()) {
            throw std::runtime_error("JSON parse error");
        }
        return doc;
    }

    // 生成JSON字符串
    template <typename T>
    static std::string stringify(const T& value) {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return buffer.GetString();
    }
};