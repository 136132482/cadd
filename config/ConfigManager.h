#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <algorithm>

class ConfigManager {
public:
    // 获取单例实例
    static ConfigManager& instance() {
        static ConfigManager instance;
        return instance;
    }

    // 加载或重载配置文件
    void load(const std::string& configPath) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!std::filesystem::exists(configPath)) {
            throw std::runtime_error("Config file not found: " + configPath);
        }

        config_path_ = configPath;
        std::ifstream fin(configPath);
        fin >> json_config_;
        apply_env_overrides();
    }

    // 安全获取配置值（支持嵌套key和默认值）
    template<typename T = nlohmann::json>
    T get(const std::string& key, const T& defaultValue = T{}) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            nlohmann::json current = json_config_;
            std::string remaining_key = key;
            size_t pos = 0;

            // 处理嵌套key (如"a.b.c")
            while ((pos = remaining_key.find('.')) != std::string::npos) {
                std::string part = remaining_key.substr(0, pos);
                if (!current.contains(part)) return defaultValue;
                current = current[part];
                remaining_key.erase(0, pos + 1);
            }

            // 最终取值
            return current.value(remaining_key, defaultValue);
        } catch (...) {
            return defaultValue;
        }
    }

    // 检查配置项是否存在
    bool has(const std::string& key) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            nlohmann::json current = json_config_;
            std::string remaining_key = key;
            size_t pos = 0;

            while ((pos = remaining_key.find('.')) != std::string::npos) {
                std::string part = remaining_key.substr(0, pos);
                if (!current.contains(part)) return false;
                current = current[part];
                remaining_key.erase(0, pos + 1);
            }

            return current.contains(remaining_key);
        } catch (...) {
            return false;
        }
    }

    // 设置配置值（自动保存）
    template<typename T>
    void set(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            nlohmann::json* current = &json_config_;
            std::string remaining_key = key;
            size_t pos = 0;

            // 处理嵌套key
            while ((pos = remaining_key.find('.')) != std::string::npos) {
                std::string part = remaining_key.substr(0, pos);
                if (!current->contains(part)) {
                    (*current)[part] = nlohmann::json::object();
                }
                current = &(*current)[part];
                remaining_key.erase(0, pos + 1);
            }

            // 设置最终值
            (*current)[remaining_key] = value;
            save_to_file();
        } catch (...) {
            throw std::runtime_error("Failed to set config value: " + key);
        }
    }

    // 获取当前配置文件路径
    std::string config_path() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_path_;
    }

    // 重新加载配置文件
    void reload() {
        if (!config_path_.empty()) {
            load(config_path_);
        }
    }

    // 获取原始JSON对象（只读）
    const nlohmann::json& get_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return json_config_;
    }

private:
    mutable std::mutex mutex_;
    nlohmann::json json_config_;
    std::string config_path_;

    // 保存配置到文件
    void save_to_file() {
        if (!config_path_.empty()) {
            std::ofstream fout(config_path_);
            if (fout) {
                fout << json_config_.dump(4); // 带缩进的格式
            }
        }
    }

    // 应用环境变量覆盖
    void apply_env_overrides() {
        // 示例：用环境变量覆盖上传目录
        if (const char* env_dir = std::getenv("UPLOAD_DIR")) {
            set("upload.base_dir", std::string(env_dir));
        }
    }
};