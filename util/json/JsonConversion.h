// utils/UniversalJsonConverter.h
#pragma once
#include <nlohmann/json.hpp>
#include <type_traits>
#include <chrono>
#include <optional>
#include <vector>
#include <map>
#include <list>
#include "../pgsql/soci_utils.h"

namespace JsonConversion {
    // 类型特征检测
    namespace detail {
        // 改进的SOCIO映射检测
        template<typename T>
        struct has_soci_mapping {
        private:
            template<typename U>
            static constexpr auto check(int) ->
            decltype(std::declval<soci::table_info<U>>().is_specialized, bool()) {
                return soci::table_info<U>::is_specialized;
            }

            template<typename>
            static constexpr bool check(...) { return false; }

        public:
            static constexpr bool value = check<T>(0);
        };

        // 容器检测
        template<typename> struct is_sequence_container : std::false_type {};
        template<typename... Ts> struct is_sequence_container<std::vector<Ts...>> : std::true_type {};
        template<typename... Ts> struct is_sequence_container<std::list<Ts...>> : std::true_type {};

        template<typename> struct is_map_container : std::false_type {};
        template<typename... Ts> struct is_map_container<std::map<Ts...>> : std::true_type {};

        // 时间点检测
        template<typename> struct is_chrono_time_point : std::false_type {};
        template<typename Clock, typename Duration>
        struct is_chrono_time_point<std::chrono::time_point<Clock, Duration>> : std::true_type {};
    }

    // 主转换模板（不再使用static_assert）
    template<typename T, typename = void>
    struct Converter {
        static nlohmann::json to_json(const T& value) {
            if constexpr (detail::has_soci_mapping<T>::value) {
                nlohmann::json j;
                soci::table_info<T>::for_each_field([&](auto field_ptr, const std::string& name) {
                    using FieldType = std::decay_t<decltype(value.*field_ptr)>;
                    j[name] = Converter<FieldType>::to_json(value.*field_ptr);
                });
                return j;
            } else {
                return nlohmann::json(value); // 回退到默认转换
            }
        }

        static T from_json(const nlohmann::json& j) {
            if constexpr (detail::has_soci_mapping<T>::value) {
                T obj;
                soci::table_info<T>::for_each_field([&](auto field_ptr, const std::string& name) {
                    if (j.contains(name)) {
                        using FieldType = std::decay_t<decltype(obj.*field_ptr)>;
                        obj.*field_ptr = Converter<FieldType>::from_json(j[name]);
                    }
                });
                return obj;
            } else {
                return j.get<T>(); // 回退到默认转换
            }
        }
    };

    // 基础类型特化（明确覆盖所有基础类型）
    template<>
    struct Converter<bool> {
        static nlohmann::json to_json(bool value) { return value; }
        static bool from_json(const nlohmann::json& j) { return j.get<bool>(); }
    };

    template<>
    struct Converter<int> {
        static nlohmann::json to_json(int value) { return value; }
        static int from_json(const nlohmann::json& j) { return j.get<int>(); }
    };

    template<>
    struct Converter<double> {
        static nlohmann::json to_json(double value) { return value; }
        static double from_json(const nlohmann::json& j) { return j.get<double>(); }
    };

    template<>
    struct Converter<std::string> {
        static nlohmann::json to_json(const std::string& value) { return value; }
        static std::string from_json(const nlohmann::json& j) { return j.get<std::string>(); }
    };

    // 时间点特化（支持所有时钟类型）
    template<typename Clock, typename Duration>
    struct Converter<std::chrono::time_point<Clock, Duration>> {
        static nlohmann::json to_json(const std::chrono::time_point<Clock, Duration>& tp) {
            auto tt = Clock::to_time_t(tp);
            std::tm tm = *std::localtime(&tt);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            return oss.str();
        }

        static std::chrono::time_point<Clock, Duration> from_json(const nlohmann::json& j) {
            std::tm tm = {};
            std::istringstream iss(j.get<std::string>());
            iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            return Clock::from_time_t(std::mktime(&tm));
        }
    };

    // optional特化（完美转发支持）
    template<typename T>
    struct Converter<std::optional<T>> {
        static nlohmann::json to_json(const std::optional<T>& opt) {
            return opt ? Converter<T>::to_json(*opt) : nullptr;
        }

        static std::optional<T> from_json(const nlohmann::json& j) {
            return j.is_null() ? std::nullopt : std::optional<T>(Converter<T>::from_json(j));
        }
    };

    // 序列容器特化（vector/list）
    template<typename T>
    struct Converter<T, std::enable_if_t<detail::is_sequence_container<T>::value>> {
        using value_type = typename T::value_type;

        static nlohmann::json to_json(const T& container) {
            nlohmann::json j = nlohmann::json::array();
            for (const auto& item : container) {
                j.push_back(Converter<value_type>::to_json(item));
            }
            return j;
        }

        static T from_json(const nlohmann::json& j) {
            T container;
            for (const auto& item : j) {
                container.insert(container.end(), Converter<value_type>::from_json(item));
            }
            return container;
        }
    };

    // 关联容器特化（map）
    template<typename T>
    struct Converter<T, std::enable_if_t<detail::is_map_container<T>::value>> {
        using key_type = typename T::key_type;
        using mapped_type = typename T::mapped_type;

        static nlohmann::json to_json(const T& container) {
            nlohmann::json j;
            for (const auto& [k, v] : container) {
                j[Converter<key_type>::to_json(k)] = Converter<mapped_type>::to_json(v);
            }
            return j;
        }

        static T from_json(const nlohmann::json& j) {
            T container;
            for (auto it = j.begin(); it != j.end(); ++it) {
                container.emplace(
                        Converter<key_type>::from_json(it.key()),
                        Converter<mapped_type>::from_json(it.value())
                );
            }
            return container;
        }
    };

    // 简化接口（完美转发支持）
    template<typename T>
    auto toJson(T&& obj) {
        return Converter<std::decay_t<T>>::to_json(std::forward<T>(obj));
    }

    template<typename T>
    auto fromJson(const nlohmann::json& j) {
        return Converter<T>::from_json(j);
    }

    // 批量转换（支持移动语义）
    template<typename T>
    nlohmann::json toJson(std::vector<T> vec) {
        nlohmann::json j = nlohmann::json::array();
        for (auto&& item : vec) {
            j.push_back(toJson(std::forward<T>(item)));
        }
        return j;
    }

    // 响应工具（支持链式调用）
    namespace Response {
        inline nlohmann::json success(nlohmann::json data = {}) {
            return {{"code", 0}, {"message", "success"}, {"data", std::move(data)}};
        }

        inline nlohmann::json error(int code, std::string msg, nlohmann::json details = {}) {
            nlohmann::json j = {{"code", code}, {"message", std::move(msg)}};
            if (!details.empty()) j["details"] = std::move(details);
            return j;
        }
    }
}