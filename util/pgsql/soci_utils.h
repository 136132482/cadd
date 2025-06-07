// soci_mapping.h
#pragma once
#include <soci/soci.h>
#include <chrono>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_set>  // 添加这个头文件
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <cstring> // 用于memset
#include <ctime>
#include <nlohmann/json.hpp>
#include <iostream>
#include "PostgisToolkit.h"
// 检测optional的特征
template<typename T>
struct is_optional : std::false_type {};
template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};
template<typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

// 检测chrono::time_point的特征
template<typename T>
struct is_chrono_time_point : std::false_type {};
template<typename Clock, typename Duration>
struct is_chrono_time_point<std::chrono::time_point<Clock, Duration>> : std::true_type {};
template<typename T>
inline constexpr bool is_chrono_time_point_v = is_chrono_time_point<T>::value;

// 用于static_assert的依赖false
template<typename T> inline constexpr bool dependent_false_v = false;


// 类型特征检测增强
template<typename T> struct is_json : std::false_type {};
template<> struct is_json<nlohmann::json> : std::true_type {};
template<typename T> inline constexpr bool is_json_v = is_json<T>::value;

template<typename T> struct is_vector_map : std::false_type {};
template<typename K, typename V>
struct is_vector_map<std::vector<std::map<K,V>>> : std::true_type {};
template<typename T> inline constexpr bool is_vector_map_v = is_vector_map<T>::value;

// 必须放在全局命名空间（避免ADL问题）
struct PostgisPoint {
    double x; // 经度 (longitude)
    double y; // 纬度 (latitude)

    // 必须包含is_valid()方法
    bool is_valid() const {
        return !std::isnan(x) && !std::isnan(y);
    }
};

template<typename T>
struct is_postgis_point {
    static constexpr bool value = false;
};

// 显式特化（必须放在相同命名空间）
template<>
struct is_postgis_point<PostgisPoint> {
    static constexpr bool value = true;
};

// C++17 简化版
template<typename T>
inline constexpr bool is_postgis_point_v = is_postgis_point<T>::value;

// 辅助模板用于static_assert
template<class> constexpr bool always_false_v = false;

// 首先确保 table_info 模板存在
namespace soci {
    template<typename T>
    struct table_info;  // 前置声明

    template<typename T>
    struct table_info {
        static constexpr bool is_specialized = false;
    };

    namespace details {
        // 添加对indicator的特化

        template<>
        struct exchange_traits<soci::indicator> {
            typedef soci::indicator base_type;
            typedef soci::indicator user_type;
            static const bool is_vectorized = false;
            static const bool is_specialized = true;
        };

        template<typename T>
        struct exchange_traits<std::optional<T>> {
            typedef typename exchange_traits<T>::base_type base_type;
            typedef typename exchange_traits<T>::user_type user_type;
            static const bool is_vectorized = exchange_traits<T>::is_vectorized;
            static const bool is_specialized = true;
        };

    }

    // 为 std::chrono::system_clock::time_point 添加类型转换支持
    template<>
    struct type_conversion<std::chrono::system_clock::time_point> {
        typedef std::tm base_type;
        static void from_base(const std::tm& t, indicator ind, std::chrono::system_clock::time_point& tp) {
            if (ind == i_null) return;
            std::tm tmp = t;
            tp = std::chrono::system_clock::from_time_t(std::mktime(&tmp));
        }

        static void to_base(const std::chrono::system_clock::time_point& tp, std::tm& t, indicator& ind) {
            auto tt = std::chrono::system_clock::to_time_t(tp);
            t = *std::localtime(&tt);
            ind = i_ok;
        }
    };

    // 为 optional<time_point> 添加类型转换支持
    template<>
    struct type_conversion<std::optional<std::chrono::system_clock::time_point>> {
        typedef std::tm base_type;
        static void from_base(const std::tm& t, indicator ind, std::optional<std::chrono::system_clock::time_point>& tp) {
            if (ind == i_null) {
                tp = std::nullopt;
            } else {
                std::tm tmp = t;
                tp = std::chrono::system_clock::from_time_t(std::mktime(&tmp));
            }
        }

        static void to_base(const std::optional<std::chrono::system_clock::time_point>& tp, std::tm& t, indicator& ind) {
            if (!tp) {
                ind = i_null;
            } else {
                auto tt = std::chrono::system_clock::to_time_t(*tp);
                t = *std::localtime(&tt);
                ind = i_ok;
            }
        }
    };

}



template<typename T>
struct type_conversion {
    using base_type = soci::values;
    static constexpr bool is_specialized = false;
    static const size_t COLUMN_NOT_FOUND = std::numeric_limits<size_t>::max();


    static size_t find_column_index(const soci::row& row, const std::string& col_name) {
        for(size_t i = 0; i < row.size(); ++i) {
            auto row_name=row.get_properties(i).get_name();
            if ( row_name== col_name) {
                return i;
            }
        }
        return COLUMN_NOT_FOUND;  //
    }

    static void from_base(const soci::row& row, soci::indicator ind, T& obj) {
        if (ind == soci::i_null) return;

        if constexpr (is_specialized) {
            type_conversion<T>::from_base(row, ind, obj);
        } else {
            size_t col_idx = 0;
            soci::table_info<T>::for_each_field([&](auto field_ptr, const std::string& col_name) {
//                if (col_idx < row.size()) {
                    try {
                        col_idx = find_column_index(row, col_name);
                        if(col_idx != COLUMN_NOT_FOUND){
                            convert_field(row, col_idx, obj.*field_ptr);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error converting field '" << col_name
                                  << "' (Type: " << typeid(decltype(obj.*field_ptr)).name() << ")"
                                  << ": " << e.what() << "\n";
                        if constexpr (is_optional_v<decltype(obj.*field_ptr)>) {
                            obj.*field_ptr = std::nullopt;
                        }
                    }
//                    col_idx++;
//                }
            });
        }
    }

    static void to_base(const T& obj, soci::values& v, soci::indicator& ind) {
        size_t col_idx = 0;
        soci::table_info<T>::for_each_field([&](auto field_ptr, const std::string&) {
            serialize_field(v, col_idx, obj.*field_ptr);
            col_idx++;
        });
        ind = soci::i_ok;
    }

private:
    // 时间转换辅助函数
    static std::chrono::system_clock::time_point from_db_time(const soci::row& row, size_t idx) {
        try {
            // 尝试直接获取time_t
            return std::chrono::system_clock::from_time_t(row.get<std::time_t>(idx));
        } catch (...) {
            // 回退到tm格式
            std::tm tm_val = row.get<std::tm>(idx);
            return std::chrono::system_clock::from_time_t(std::mktime(&tm_val));
        }
    }



    template<typename FieldType>
    static void convert_field(const soci::row& row, size_t idx, FieldType& field) {
        if constexpr (is_json_v<FieldType>) {
            if (row.get_indicator(idx) != soci::i_null) {
                auto json_str = row.get<std::string>(idx);
                field = nlohmann::json::parse(json_str);
            } else {
                field = nlohmann::json();
            }
        } else if constexpr (is_vector_map_v<FieldType>) {
            using ValueType = typename FieldType::value_type;
            auto json_str = row.get<std::string>(idx);
            auto json_array = nlohmann::json::parse(json_str);
            for (auto& item : json_array) {
                ValueType map_item;
                for (auto& [k, v] : item.items()) {
                    map_item[k] = v.get<typename ValueType::mapped_type>();
                }
                field.push_back(map_item);
            }
        }
        else if constexpr (is_optional_v<FieldType>) {
            try {
                using ValueType = typename FieldType::value_type;
                if (row.get_indicator(idx) != soci::i_null) {
                     if constexpr (is_chrono_time_point_v<ValueType>) {
                        field = from_db_time(row, idx);
                    } else {
                        field = row.get<ValueType>(idx);
                    }
                } else {
                    field = std::nullopt;
                }
            }catch (const std::exception& e) {
                std::cerr << "Type conversion failed for field at index " << idx
                          << ": " << e.what() << std::endl;
                field = std::nullopt;
            }
        } else if constexpr (is_chrono_time_point_v<FieldType>) {
            field = from_db_time(row, idx);
        } else {
            field = row.get<FieldType>(idx);
        }
    }



    template<typename FieldType>
    static void serialize_field(soci::values& v, size_t idx, const FieldType& field) {

        if constexpr (is_json_v<FieldType>) {
            if (!field.empty()) {
                v.set(idx, field.dump());
            } else {
                set_null(v, idx);
            }
        }
//         else if constexpr (is_vector_map_v<FieldType>) {
//            nlohmann::json json_array;
//            for (const auto& map_item : field) {
//                nlohmann::json json_obj;
//                for (const auto& [k, val] : map_item) {
//                    json_obj[k] = val;
//                }
//                json_array.push_back(json_obj);
//            }
//            v.set(idx, json_array.dump(), soci::i_ok);
//        }
        else if constexpr (is_optional_v<FieldType>) {
            if (field.has_value()) {
                if constexpr (is_chrono_time_point_v<typename FieldType::value_type>) {
                    v.set(idx, std::chrono::system_clock::to_time_t(*field));
                } else {
                    v.set(idx, *field);
                }
            } else {
                set_null(v, idx);
            }
        } else if constexpr (is_chrono_time_point_v<FieldType>) {
            v.set(idx, std::chrono::system_clock::to_time_t(field));
        } else if constexpr (std::is_same_v<FieldType, const char*>) {
            v.set(idx, field);
        } else if constexpr (std::is_same_v<FieldType, std::string>) {
            v.set(idx, field.c_str());
        } else {
            v.set(idx, field);
        }
    }

    static void set_null(soci::values& v, size_t idx) {
#if SOCI_VERSION >= 400
        v.set_null(idx);
#else
        soci::indicator ind = soci::i_null;
        v.set(idx, ind);
#endif
    }
};

// 添加基础字段配置
namespace soci_config {
    inline bool is_standard_field(const std::string& name,  bool is_primary = false) {
        static const std::unordered_set<std::string> std_fields = {
                "id",
//                "created_at",
//                "updated_at",
//                "is_delete"
        };
        return is_primary ||std_fields.count(name);
    }
}

namespace soci_helpers {

    template<typename T>
    struct IdConverter {
        static std::string toSQL(T id) {
            return std::to_string(id);
        }

        static T fromSQL(const soci::row& r, size_t index) {
            return r.get<T>(index);
        }
    };

    // 特化处理 optional 类型
    template<typename T>
    struct IdConverter<std::optional<T>> {
        static std::string toSQL(const std::optional<T>& id) {
            return id ? std::to_string(*id) : "NULL";
        }

        static std::optional<T> fromSQL(const soci::row& r, size_t index) {
            return r.get<std::optional<T>>(index);
        }
    };

    template<typename> struct is_pair : std::false_type {};
    template<typename T1, typename T2>
    struct is_pair<std::pair<T1, T2>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_pair_v = is_pair<T>::value;


    // C++17兼容的编译时tuple遍历
    template<typename Tuple, typename F, size_t... I>
    constexpr void tuple_for_each_impl(Tuple&& t, F&& f, std::index_sequence<I...>) {
        using swallow = int[];
        (void)swallow{0, (f(std::get<I>(t)), 0)...};
    }

    template<typename Tuple, typename F>
    constexpr void tuple_for_each(Tuple&& t, F&& f) {
        constexpr size_t size = std::tuple_size_v<std::decay_t<Tuple>>;
        tuple_for_each_impl(std::forward<Tuple>(t),
                            std::forward<F>(f),
                            std::make_index_sequence<size>{});
    }

    // 编译时tuple遍历工具
    template<size_t N>
    struct TupleForEach {
        template<typename Tuple, typename F>
        static constexpr void apply(Tuple&& t, F&& f) {
            TupleForEach<N-1>::apply(t, std::forward<F>(f));
            f(std::get<N-1>(std::forward<Tuple>(t)));
        }
    };

    template<>
    struct TupleForEach<0> {
        template<typename Tuple, typename F>
        static constexpr void apply(Tuple&&, F&&) {}
    };


    // 修改后的make_mapping_pair实现
    template<typename T1, typename T2>
    constexpr auto make_pair(T1&& t1, T2&& t2) {
        return std::make_pair(std::forward<T1>(t1), std::forward<T2>(t2));
    }

    template<typename T1, typename T2, typename T3>
    constexpr auto make_pair(T1&& t1, T2&& t2, T3&& t3) {
        return std::make_tuple(std::forward<T1>(t1), std::forward<T2>(t2), std::forward<T3>(t3));
    }



    // 时间转换函数
    inline std::string to_db_string(const std::chrono::system_clock::time_point& tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    inline std::chrono::system_clock::time_point from_db_string(const std::string& s) {
        std::tm tm = {};
        std::istringstream iss(s);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }



    // 时间转换辅助函数
    inline std::tm to_tm(const std::chrono::system_clock::time_point& tp) {
        auto tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = {};
        #ifdef _WIN32
                localtime_s(&tm, &tt);
        #else
                localtime_r(&tt, &tm);
        #endif
                return tm;
    }

    // 时间点转换为数据库时间戳（time_t）
    inline std::time_t to_db_timestamp(const std::chrono::system_clock::time_point& tp) {
        return std::chrono::system_clock::to_time_t(tp);
    }

    // 数据库时间戳转换为时间点
    inline std::chrono::system_clock::time_point from_db_timestamp(std::time_t tt) {
        return std::chrono::system_clock::from_time_t(tt);
    }


    // 1. 先声明主模板
    template<typename T>
    struct TypeConverter;

    // 为 std::optional 的特化必须在同一命名空间
    template<typename T>
    struct TypeConverter<std::optional<T>> {
        static std::string toSQL(const std::optional<T>& value, bool enable_postgis = false) {
            return value.has_value() ? TypeConverter<T>::toSQL(*value,enable_postgis) : "NULL";
        }
    };



    // 类型转换器
    template<typename T>
    struct TypeConverter {
        static void to_base(const std::chrono::system_clock::time_point& tp,
                            soci::values& v, const std::string& col_name) {
            v.set(col_name, to_db_timestamp(tp), soci::i_ok);
        }
        static std::chrono::system_clock::time_point from_base(const soci::values& v,
                                                               const std::string& col_name) {
            return from_db_timestamp(v.get<std::time_t>(col_name));
        }



        static std::string toSQL(const T& value, bool enable_postgis = false) {
            if constexpr (is_postgis_point_v<T>&&enable_postgis) {
                if (!value.is_valid()) return "NULL";
                return "ST_GeomFromText('POINT(" +
                       std::to_string(value.x) + " " +
                       std::to_string(value.y) + ")', 4326)";
            }
           else if constexpr (is_chrono_time_point_v<T>) {
                auto tm = to_tm(value);
                std::ostringstream oss;
                oss << std::put_time(&tm, "'%Y-%m-%d %H:%M:%S+00'");
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                if (GeometryConverter::isPointString(value)&&enable_postgis) {
                    return "ST_GeomFromText('" + value + "', 4326)";
                }
                return value.empty() ? "NULL" :
                       "'" + boost::replace_all_copy(value, "'", "''") + "'";
            }
            else if constexpr (std::is_same_v<T, const char*>) {
                return value == nullptr ? "NULL" :
                       "'" + boost::replace_all_copy(std::string(value), "'", "''") + "'";
            }
            else if constexpr (is_optional_v<T>) {
                return value.has_value() ? toSQL(*value) : "NULL";
            }
            else if constexpr (std::is_integral_v<T>) {
                if constexpr (std::is_same_v<T, bool>) {
                    return value ? "1" : "0";
                }
                return std::to_string(value);
            }
            else if constexpr (std::is_floating_point_v<T>) {
                std::ostringstream oss;
                oss << value;
                return oss.str();
            }
            else if constexpr (std::is_enum_v<T>) {
                return std::to_string(static_cast<std::underlying_type_t<T>>(value));
            }
            else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                if (value.empty()) return "NULL";
                static const char* hex_digits = "0123456789ABCDEF";
                std::string hex;
                hex.reserve(value.size() * 2);
                for (uint8_t byte : value) {
                    hex += hex_digits[(byte >> 4) & 0x0F];
                    hex += hex_digits[byte & 0x0F];
                }
                return "x'" + hex + "'";
            }
            else {
                static_assert(dependent_false_v<T>, "Unsupported type for SQL conversion");
                return "NULL";
            }
        }




        // 类型转换到 JSON（替代原来的 toSQL）
        static nlohmann::json to_json(const T& value) {
            if constexpr (std::is_same_v<T, std::tm>) {
                std::ostringstream oss;
                oss << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
                return oss.str();
            }
            else if constexpr (is_chrono_time_point_v<T>) {
                // 时间点 -> ISO 8601 字符串
                auto tm = to_tm(value);
                std::ostringstream oss;
                oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return value.empty() ? nullptr : value; // nullptr 对应 JSON 的 null
            }
            else if constexpr (std::is_same_v<T, const char*>) {
                return value == nullptr ? nullptr : std::string(value);
            }
            else if constexpr (is_optional_v<T>) {
                return value.has_value() ? to_json(*value) : nullptr;
            }
            else if constexpr (std::is_integral_v<T>) {
                if constexpr (std::is_same_v<T, bool>) {
                    return value; // 布尔值直接返回
                }
                return value; // 整数直接返回
            }
            else if constexpr (std::is_floating_point_v<T>) {
                return value; // 浮点数直接返回
            }
            else if constexpr (std::is_enum_v<T>) {
                return static_cast<std::underlying_type_t<T>>(value); // 枚举转底层类型
            }
            else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                if (value.empty()) return nullptr;
                // 二进制数据转为 Base64（需外部库支持，此处简化）
                return "BINARY_DATA"; // 实际项目中可用 Base64 编码
            }
            else {
                static_assert(dependent_false_v<T>, "Unsupported type for JSON conversion");
                return nullptr;
            }
        }

    };

    // 字段访问器
    template<typename T>
    class FieldAccessor {
        const T& obj;
        const std::string& target_col;
        std::string result;

    public:
        FieldAccessor(const T& o, const std::string& col)
                : obj(o), target_col(col) {}

        template<typename MemberPtr, typename ColName>
        void operator()(MemberPtr ptr, ColName name) {
            if (name == target_col) {
                using MemberType = std::decay_t<decltype(obj.*ptr)>;
                result = TypeConverter<MemberType>::toSQL(obj.*ptr);
            }
        }

        [[nodiscard]] std::string getResult() const { return result; }
    };


    //字符串时间转换
    //            if (is_from_db) {
    //                obj.*field_ptr = from_db_string(v.get<std::string>(col_name));
    //            } else {
    //                v.set(col_name, to_db_string(obj.*field_ptr), soci::i_ok);
    //            }

    inline bool column_exists(const soci::values& v, const std::string& col_name) {
        // SOCI 兼容的字段检查方法
        try {
            // 尝试获取列，如果不存在会抛出异常
            v.get_indicator(col_name);
            return true;
        } catch (const soci::soci_error&) {
            return false;
        }
    }


    // 字段处理函数
    template<typename Struct, typename FieldPtr>
    void process_field(soci::values& v, Struct& obj,
                       FieldPtr field_ptr, const std::string& col_name,
                       bool is_from_db) {
        using FieldType = std::decay_t<decltype(obj.*field_ptr)>;
        const bool column_exists = soci_helpers::column_exists(v, col_name);

        if constexpr (is_optional_v<FieldType>) {
            if (!column_exists) {
                obj.*field_ptr = std::nullopt;
                return;
            }
            using ValueType = typename FieldType::value_type;
            if (is_from_db) {
                try {
                    if constexpr (is_chrono_time_point_v<ValueType>) {
                        std::tm tm_val;
                        soci::indicator ind = v.get_indicator(col_name);
                        if (ind == soci::i_ok) {
                            tm_val = v.get<std::tm>(col_name);
                            obj.*field_ptr = std::chrono::system_clock::from_time_t(std::mktime(&tm_val));
                            }
                    } else {
                        ValueType tmp;
                        v.get(col_name, tmp);
                        obj.*field_ptr = tmp;
                    }
                } catch (...) {
                    obj.*field_ptr = std::nullopt;
                }
            } else {
                if ((obj.*field_ptr).has_value()) {
                    if constexpr (is_chrono_time_point_v<ValueType>) {
                        v.set(col_name,
                              std::chrono::system_clock::to_time_t(*(obj.*field_ptr)),
                              soci::i_ok);
                    } else {
                        v.set(col_name, *(obj.*field_ptr), soci::i_ok);
                    }
                } else {
                    // 处理空值的三种情况
                    if constexpr (std::is_default_constructible_v<ValueType>) {
                        // 情况1：类型可默认构造
                        v.set(col_name, ValueType{}, soci::i_null);
                    } else if constexpr (std::is_same_v<ValueType, std::string>) {
                        // 情况2：字符串特殊处理
                        v.set(col_name, std::string{}, soci::i_null);
                    } else {
                        // 情况3：其他不可构造类型
                        soci::indicator ind = soci::i_null;
                        v.set(col_name, ind, ind);
                    }
                }
            }
        }
        else if constexpr (is_chrono_time_point_v<FieldType>) {
            if (is_from_db) {
                std::time_t tmp;
                v.get(col_name, tmp);
                obj.*field_ptr = std::chrono::system_clock::from_time_t(tmp);
            } else {
                v.set(col_name,
                      std::chrono::system_clock::to_time_t(obj.*field_ptr),
                      soci::i_ok);
            }
        }
        else {
            if (is_from_db) {
                v.get(col_name, obj.*field_ptr);
            } else {
                // 处理 const char* 特殊转换
                if constexpr (std::is_same_v<FieldType, const char*>) {
                    v.set(col_name, std::string(obj.*field_ptr), soci::i_ok);
                } else {
                    v.set(col_name, obj.*field_ptr, soci::i_ok);
                }
            }
        }
    }

    template<typename Fn, typename Tuple, size_t... I>
    void apply_impl(Fn&& fn, Tuple&& t, std::index_sequence<I...>) {
        // 对每个元素进行转换
        ([&] {
            auto&& element = std::get<I>(std::forward<Tuple>(t));
            using ElementType = std::decay_t<decltype(element)>;

            if constexpr (std::is_same_v<ElementType, const char*>) {
                // 将 const char* 列名转为 std::string
                fn(std::string(element));
            } else {
                // 其他类型直接传递
                fn(element);
            }
        }(), ...);
    }

    template<typename Fn, typename Tuple>
    void apply(Fn&& fn, Tuple&& t) {
        constexpr auto size = std::tuple_size_v<std::decay_t<Tuple>>;
        apply_impl(std::forward<Fn>(fn), std::forward<Tuple>(t),
                   std::make_index_sequence<size>{});
    }
}

#define SOCI_MAP(StructName, TableName, ...) \
namespace soci { \
    template<> \
    struct table_info<StructName> {          \
        static constexpr bool is_specialized = true; \
        using type = StructName; \
        \
        static constexpr const char* name = TableName; \
        \
        static constexpr auto make_mapping() { \
            return std::make_tuple(__VA_ARGS__); \
        } \
        \
        struct PrimaryKeyInfo { \
            static constexpr const char* column() { \
                constexpr auto mapping = make_mapping(); \
                const char* result = "id"; \
                \
                soci_helpers::tuple_for_each(mapping, [&](auto&& field) { \
                    using FieldType = std::decay_t<decltype(field)>; \
                    if constexpr (soci_helpers::is_pair_v<FieldType>) { \
                        /* 忽略pair */ \
                    } else if constexpr (std::tuple_size_v<FieldType> > 2) { \
                        if (std::get<2>(field)) result = std::get<1>(field); \
                    } \
                }); \
                return result; \
            } \
            \
            template<typename U = StructName> \
            static constexpr auto member() { \
                constexpr auto mapping = make_mapping(); \
                decltype(&U::id) result = &U::id; \
                \
                soci_helpers::tuple_for_each(mapping, [&](auto&& field) { \
                    using FieldType = std::decay_t<decltype(field)>; \
                    if constexpr (soci_helpers::is_pair_v<FieldType>) { \
                        /* 忽略pair */ \
                    } else if constexpr (std::tuple_size_v<FieldType> > 2) { \
                        if (std::get<2>(field)) result = std::get<0>(field); \
                    } \
                }); \
                return result; \
            } \
        }; \
        \
        static constexpr const char* id_column() { \
            return PrimaryKeyInfo::column(); \
        } \
        \
        template<typename U = StructName> \
        static constexpr auto id_member() { \
            return PrimaryKeyInfo::template member<U>(); \
        } \
        \
        static std::vector<std::string> columns(bool exclude_standard = false) { \
            std::vector<std::string> names; \
            auto mapping = make_mapping(); \
            \
            soci_helpers::tuple_for_each(mapping, [&](auto&& field) { \
                if constexpr (std::tuple_size_v<std::decay_t<decltype(field)>> >= 2) { \
                    std::string col_name = std::get<1>(field); \
                    if (!exclude_standard || !soci_config::is_standard_field(col_name)) { \
                        names.push_back(col_name); \
                    } \
                } \
            }); \
            return names; \
        } \
        \
        template <typename F> \
        static void for_each_field_with_primary(F&& func) { \
            auto mapping = make_mapping(); \
            soci_helpers::apply([&](auto&& field) { \
                using field_type = std::decay_t<decltype(field)>; \
                if constexpr (std::tuple_size_v<field_type> >= 2) { \
                    auto&& member_ptr = std::get<0>(field); \
                    std::string col_name = std::get<1>(field); \
                    bool is_primary = false; \
                    if constexpr (std::tuple_size_v<field_type> > 2) { \
                        is_primary = std::get<2>(field); \
                    } \
                    func(member_ptr, col_name, is_primary); \
                } \
            }, mapping); \
        } \
        \
        /* 版本2：明确不带bool参数的版本 */ \
        template <typename F> \
        static void for_each_field(F&& func) { \
            auto mapping = make_mapping(); \
            soci_helpers::apply([&](auto&& field) { \
                using field_type = std::decay_t<decltype(field)>; \
                if constexpr (std::tuple_size_v<field_type> >= 2) { \
                    auto&& member_ptr = std::get<0>(field); \
                    std::string col_name = std::get<1>(field); \
                    func(member_ptr, col_name); \
                } \
            }, mapping); \
        } \
    }; \
    \
    template<> \
    struct type_conversion<StructName> { \
        typedef values base_type; \
        \
        static void from_base(values const& v, indicator ind, StructName& obj) { \
            if (ind == i_null) return; \
            \
            auto process = [&](auto&& field) { \
                if constexpr (std::tuple_size_v<std::decay_t<decltype(field)>> >= 2) { \
                    soci_helpers::process_field( \
                        const_cast<values&>(v), obj, \
                        std::get<0>(field), \
                        std::string(std::get<1>(field)), \
                        true \
                    ); \
                } \
            }; \
            \
            auto mapping = table_info<StructName>::make_mapping(); \
            soci_helpers::apply(process, mapping); \
        } \
        \
        static void to_base(StructName const& obj, values& v, indicator& ind) { \
            auto process = [&](auto&& field) { \
                if constexpr (std::tuple_size_v<std::decay_t<decltype(field)>> >= 2) { \
                    if (!soci_config::is_standard_field(std::get<1>(field))) { \
                        soci_helpers::process_field( \
                            v, \
                            const_cast<StructName&>(obj), \
                            std::get<0>(field), \
                            std::string(std::get<1>(field)), \
                            false \
                        ); \
                    } \
                } \
            }; \
            \
            auto mapping = table_info<StructName>::make_mapping(); \
            soci_helpers::apply(process, mapping); \
            ind = i_ok; \
        } \
    }; \
}


