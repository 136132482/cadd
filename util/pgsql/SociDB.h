#pragma once

#include <soci/soci.h>
#include <soci/postgresql/soci-postgresql.h>
#include <memory>
#include <optional>
#include <vector>
#include <functional>
#include <iostream>
#include <chrono>
#include <tuple>
#include <utility>
#include <sstream>
#include <algorithm>
#include <string>
#include <boost/algorithm/string/join.hpp>
#include "soci_utils.h"
#include "../Logger/Logger.h"


class SociDB {
public:
    template<typename T>
    struct PageResult {
        std::vector<T> items;
        int current_page;
        int page_size;
        int total_items;
        int total_pages;
    };

    static void init(const std::string& connStr="host=localhost dbname=pg_db user=root password=root options='-c timezone=Asia/Shanghai'",
                     size_t poolSize = 5) {
        // 直接使用全局Logger实例
        Logger::Global().Info("Initializing database connection pool");
        Logger::Global().Debug("Connection string: " + connStr);
        Logger::Global().Debug("Pool size: " + std::to_string(poolSize));

        getPool() = std::make_shared<soci::connection_pool>(poolSize);
        for(size_t i = 0; i < poolSize; ++i) {
            try {
                getPool()->at(i).open(soci::postgresql, connStr);
                Logger::Global().Debug("Successfully opened connection #" + std::to_string(i));
            } catch(const std::exception& e) {
                Logger::Global().Error("Failed to open connection #" + std::to_string(i) + ": " + e.what());
                throw;
            }
        }

        Logger::Global().Info("Database connection pool initialized successfully");
    }

    class Session {
    public:
        Session() : pos_(getPool()->lease()), sql_(getPool()->at(pos_)) {
            Logger::Global().Debug("Acquired database connection from pool, position: " + std::to_string(pos_));
        }

        ~Session() {
            try {
                getPool()->give_back(pos_);
                Logger::Global().Debug("Released database connection back to pool, position: " + std::to_string(pos_));
            }
            catch(const std::exception& e) {
                Logger::Global().Error("Failed to release connection: " + std::string(e.what()));
            }
        }
        soci::session& get() { return sql_; }

    private:
        size_t pos_;
        soci::session& sql_;

    };



    template<typename T>
    static bool insert(const T& obj) {
        Session sess;
        soci::transaction tr(sess.get());
        try {
            const auto& table = soci::table_info<T>::name;
            const auto& columns = soci::table_info<T>::columns(true);

            // 1. 准备参数值
            std::vector<std::string> params;
            std::map<std::string, std::string> param_values;

            // 处理每个字段
            soci::table_info<T>::for_each_field(
                    [&](auto member_ptr, const std::string& col_name) {
                        if (!soci_config::is_standard_field(col_name)) {
                            using MemberType = std::decay_t<decltype(obj.*member_ptr)>;
                            std::string val = soci_helpers::TypeConverter<MemberType>::toSQL(obj.*member_ptr);
                            params.push_back(val);
                            param_values[col_name] = val;
                        }
                    }
            );

            // 准备插入值（自动跳过标准字段）
//            soci::values v;
//            soci::indicator ind = soci::i_ok;
//            soci::type_conversion<T>::to_base(obj, v, ind);
//
//            // 使用FieldAccessor获取类型信息
//            soci_helpers::FieldAccessor<T> accessor(obj, "");
//            soci::table_info<T>::for_each_field([&](auto field_ptr, const std::string& col_name) {
//                if (!soci_config::is_standard_field(col_name)) {
//                    try {
//                        T temp_obj;
//                        soci_helpers::process_field(v, temp_obj, field_ptr, col_name, true);
//                        soci_helpers::FieldAccessor<T> accessor(temp_obj, col_name);
//                        accessor(field_ptr, col_name);
//                        std::cout << col_name << " = " << accessor.getResult() << "\n";
//                    } catch (const std::exception& e) {
//                        std::cout << col_name << " = [ERROR: " << e.what() << "]\n";
//                    }
//                }
//            });

            // 2. 构建SQL（使用ostringstream避免类型问题）
            std::ostringstream sql;
            sql << "INSERT INTO " << table << " ("
                << boost::algorithm::join(columns, ", ") << ") VALUES ("
                << boost::algorithm::join(params, ", ") << ")";

            std::string final_sql = sql.str();

            // 3. 打印调试信息
            std::cout << "\n===== 单条插入 =====" << std::endl;
            std::cout << "表名: " << table << std::endl;
            std::cout << "列名: " << boost::algorithm::join(columns, ", ") << "\n" << std::endl;

            std::cout << "--- 参数值 ---" << std::endl;
            for (const auto& [col, val] : param_values) {
                std::cout << "  " << std::left << std::setw(20) << col
                          << " = " << val << std::endl;
            }

            std::cout << "\n--- 完整执行SQL ---" << std::endl;
            std::cout << final_sql << "\n" << std::endl;

            // 4. 执行SQL
            auto start_time = std::chrono::steady_clock::now();

            sess.get() << final_sql;

            auto duration = std::chrono::steady_clock::now() - start_time;

            std::cout << "--- 执行结果 ---" << std::endl;
            std::cout << "影响行数: 1, 耗时: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                      << "ms\n" << std::endl;

            tr.commit();
            return true;

        } catch(const std::exception& e) {
            tr.rollback();
            std::cerr << "\n!!!!! 单条插入错误 !!!!!" << std::endl;
            std::cerr << "错误信息: " << e.what() << std::endl;
            return false;
        }
    }

    template<typename T>
    static bool bulkInsert(std::vector<T>& objects) {
        if (objects.empty()) {
            std::cout << "[INFO] 空数据集，跳过批量插入" << std::endl;
            return true;
        }

        Session sess;
        soci::transaction tr(sess.get());

        try {
            const auto& table = soci::table_info<T>::name;
            const auto& columns = soci::table_info<T>::columns(true);

            // 打印表结构信息
            std::cout << "\n===== 批量插入表结构 =====" << std::endl;
            std::cout << "表名: " << table << std::endl;
            std::cout << "列名: " << boost::algorithm::join(columns, ", ") << "\n" << std::endl;

            // 1. 构建基础INSERT语句
            std::ostringstream sql_base;
            sql_base << "INSERT INTO " << table << " ("
                     << boost::algorithm::join(columns, ", ") << ") VALUES ";

            // 2. 分批次处理
            const size_t BATCH_SIZE = 500;
            for (size_t offset = 0; offset < objects.size(); offset += BATCH_SIZE) {
                auto end_pos = std::min(offset + BATCH_SIZE, objects.size());
                std::vector<std::string> value_groups;
                std::vector<std::map<std::string, std::string>> param_values;

                // 打印批次信息
                std::cout << "===== 批量插入批次 " << (offset/BATCH_SIZE + 1)
                          << " (记录 " << offset+1 << "-" << end_pos << ") =====\n" << std::endl;

                for (size_t i = offset; i < end_pos; ++i) {
                    std::vector<std::string> params;
                    std::map<std::string, std::string> record_params;

                    // 处理每个字段
                    soci::table_info<T>::for_each_field(
                            [&](auto member_ptr, const std::string& col_name) {
                                if (std::find(columns.begin(), columns.end(), col_name) != columns.end()) {
                                    using MemberType = std::decay_t<decltype(objects[i].*member_ptr)>;
                                    std::string val = soci_helpers::TypeConverter<MemberType>::toSQL(objects[i].*member_ptr);
                                    params.push_back(val);
                                    record_params[col_name] = val;
                                }
                            }
                    );
                    value_groups.push_back("(" + boost::algorithm::join(params, ", ") + ")");
                    param_values.push_back(record_params);
                }

                // 3. 打印参数详情
                std::cout << "--- 参数值 ---" << std::endl;
                for (size_t i = 0; i < param_values.size(); ++i) {
                    std::cout << "记录 " << offset + i + 1 << ":" << std::endl;
                    for (const auto& [col, val] : param_values[i]) {
                        std::cout << "  " << std::left << std::setw(20) << col
                                  << " = " << val << std::endl;
                    }
                    if (i < param_values.size() - 1) {
                        std::cout << std::string(50, '-') << std::endl;
                    }
                }

                // 构建完整SQL
                std::string sql = sql_base.str() + boost::algorithm::join(value_groups, ", ");

                // 4. 打印完整SQL（不再截断）
                std::cout << "\n--- 完整执行SQL ---" << std::endl;
                std::cout << sql << "\n" << std::endl;

                // 5. 执行SQL并计时
                auto start_time = std::chrono::steady_clock::now();
                sess.get() << sql;
                auto duration = std::chrono::steady_clock::now() - start_time;

                std::cout << "--- 执行结果 ---" << std::endl;
                std::cout << "影响行数: " << (end_pos - offset)
                          << ", 耗时: "
                          << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                          << "ms\n" << std::endl;
            }

            tr.commit();
            std::cout << "\n===== 批量插入完成 =====" << std::endl;
            std::cout << "总记录数: " << objects.size() << std::endl;
            return true;

        } catch(const std::exception& e) {
            tr.rollback();
            std::cerr << "\n!!!!! 批量插入错误 !!!!!" << std::endl;
            std::cerr << "错误信息: " << e.what() << std::endl;
            return false;
        }
    }



        /* 批量修改 */
        template<typename T>
        static bool bulkUpdate(const std::vector<T>& objects) {
            if (objects.empty()) {
                std::cout << "[INFO] 空数据集，跳过批量更新" << std::endl;
                return true;
            }

            // 打印全局信息
            std::cout << "\n===== 开始批量更新 =====" << std::endl;
            std::cout << "总记录数: " << objects.size() << std::endl;

            Session sess;
            soci::transaction tr(sess.get());
            try {
                const auto& table = soci::table_info<T>::name;
                const auto& columns = soci::table_info<T>::columns(true);
                const auto& id_column = soci::table_info<T>::id_column();
                auto id_member_ptr = soci::table_info<T>::template id_member<T>();

                // 表结构信息
                std::cout << "\n[表结构]" << std::endl;
                std::cout << "表名: " << table << std::endl;
                std::cout << "主键列: " << id_column << std::endl;
                std::cout << "可更新列: " << boost::algorithm::join(columns, ", ") << "\n" << std::endl;

                // 分批次处理
                const size_t BATCH_SIZE = 500;
                size_t success_count = 0;
                auto total_start = std::chrono::steady_clock::now();

                for (size_t offset = 0; offset < objects.size(); offset += BATCH_SIZE) {
                    auto end_pos = std::min(offset + BATCH_SIZE, objects.size());
                    auto batch_start = std::chrono::steady_clock::now();

                    std::cout << "── 批次 " << (offset/BATCH_SIZE + 1)
                              << " (" << offset+1 << "-" << end_pos << ") ──" << std::endl;

                    for (size_t i = offset; i < end_pos; ++i) {
                        const auto& obj = objects[i];
                        auto record_start = std::chrono::steady_clock::now();

                        // 获取ID值
                        using MemberType = std::decay_t<decltype(obj.*id_member_ptr)>;
                        auto id_value = soci_helpers::TypeConverter<MemberType>::toSQL(obj.*id_member_ptr);

                        // ID检查
                        if (id_value.empty() || id_value == "0" || id_value == "NULL") {
                            std::cout << "⚠️ 记录无ID，转为插入操作" << std::endl;
                            insert(obj);
                            continue;
                        }

                        int exists = 0;
                        sess.get() << "SELECT 1 FROM " << table << " WHERE " << id_column << " = " << id_value,
                                soci::into(exists);

                        if (!exists) {
                            std::cout << "❌ 记录ID不存在: " << id_value << "\n";
                            continue;
                        }

                        // 构建SET子句（过滤NULL）
                        std::vector<std::string> set_clauses;
                        std::map<std::string, std::string> param_values;

                        soci::table_info<T>::for_each_field(
                                [&](auto member_ptr, const std::string& col_name) {
                                    if (!soci_config::is_standard_field(col_name) && col_name != id_column) {
                                        using FieldType = std::decay_t<decltype(obj.*member_ptr)>;
                                        std::string val = soci_helpers::TypeConverter<FieldType>::toSQL(obj.*member_ptr);
                                        // 关键修改：过滤NULL值
                                        if (val != "NULL") {
                                            set_clauses.push_back(col_name + " = " + val);
                                            param_values[col_name] = val;
                                        }
                                    }
                                }
                        );

                        // 构建SQL
                        std::ostringstream sql;
                        sql << "UPDATE " << table << " SET "
                            << boost::algorithm::join(set_clauses, ", ")
                            << " WHERE " << id_column << " = " << id_value;

                        std::string final_sql = sql.str();

                        // 打印记录详情
                        std::cout << "\n  [记录 " << i+1 << "]" << std::endl;
                        std::cout << "  ├─ 主键: " << id_column << " = " << id_value << std::endl;
                        std::cout << "  ├─ 更新字段:" << (param_values.empty() ? " (无)" : "") << std::endl;

                        for (const auto& [col, val] : param_values) {
                            std::cout << "  │   " << std::left << std::setw(15) << col
                                      << " = " << val << std::endl;
                        }

                        std::cout << "  └─ SQL: " << final_sql << std::endl;

                        // 执行更新
                        try {
                            sess.get() << final_sql;
                            success_count++;

                            auto duration = std::chrono::steady_clock::now() - record_start;
                            std::cout << "  ✅ 更新成功 ("
                                      << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                                      << "ms)" << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "  ❌ 更新失败: " << e.what() << std::endl;
                        }
                    }

                    // 批次统计
                    auto batch_duration = std::chrono::steady_clock::now() - batch_start;
                    std::cout << "\n批次完成: " << (end_pos-offset) << "条, 耗时: "
                              << std::chrono::duration_cast<std::chrono::milliseconds>(batch_duration).count()
                              << "ms\n" << std::endl;
                }

                // 最终统计
                tr.commit();
                auto total_duration = std::chrono::steady_clock::now() - total_start;

                std::cout << "\n===== 批量更新完成 =====" << std::endl;
                std::cout << "成功数: " << success_count << "/" << objects.size() << std::endl;
                std::cout << "总耗时: "
                          << std::chrono::duration_cast<std::chrono::seconds>(total_duration).count()
                          << "s" << std::endl;

                return success_count == objects.size();

            } catch(const std::exception& e) {
                tr.rollback();
                std::cerr << "\n!!!!! 批量更新错误 !!!!!" << std::endl;
                std::cerr << "错误信息: " << e.what() << std::endl;
                return false;
            }
        }


    /* 单条修改 */
    template<typename T>
    static bool update(const T& obj) {
        auto start_time = std::chrono::steady_clock::now();
        Session sess;
        soci::transaction tr(sess.get());

        try {
            // 1. 获取元信息
            const auto& table = soci::table_info<T>::name;
            const auto& id_column = soci::table_info<T>::id_column();
            auto id_member_ptr = soci::table_info<T>::template id_member<T>();

            // 2. 检查ID有效性
            using IdType = std::decay_t<decltype(obj.*id_member_ptr)>;
            auto id_value = soci_helpers::TypeConverter<IdType>::toSQL(obj.*id_member_ptr);

            std::cout << "\n===== 单条更新开始 =====" << std::endl;
            std::cout << "表名: " << table << std::endl;
            std::cout << "主键: " << id_column << " = " << id_value << "\n";

            if (id_value.empty() || id_value == "0" || id_value == "NULL") {
                std::cout << "⚠️ 检测到空ID，自动转为插入操作\n" << std::endl;
                return insert(obj);
            }
            // 3. 检查ID是否存在
            int exists = 0;
            sess.get() << "SELECT 1 FROM " << table << " WHERE " << id_column << " = " << id_value,
                    soci::into(exists);

            if (!exists) {
                throw std::runtime_error("更新失败：ID " + id_value + " 不存在");
            }

            // 3. 构建更新语句（过滤NULL值）
            std::vector<std::string> set_clauses;
            std::map<std::string, std::string> updated_fields;

            soci::table_info<T>::for_each_field(
                    [&](auto member_ptr, const std::string& col_name) {
                        if (!soci_config::is_standard_field(col_name) && col_name != id_column) {
                            using FieldType = std::decay_t<decltype(obj.*member_ptr)>;
                            std::string val = soci_helpers::TypeConverter<FieldType>::toSQL(obj.*member_ptr);
                            // 关键改进：跳过NULL值
                            if (val != "NULL") {
                                set_clauses.push_back(col_name + " = " + val);
                                updated_fields[col_name] = val;
                            }
                        }
                    }
            );

            // 4. 构建最终SQL
            std::ostringstream sql;
            sql << "UPDATE " << table << " SET "
                << boost::algorithm::join(set_clauses, ", ")
                << " WHERE " << id_column << " = " << id_value;

            std::string final_sql = sql.str();

            // 5. 打印调试信息
            std::cout << "─── 更新详情 ───" << std::endl;
            if (updated_fields.empty()) {
                std::cout << "未检测到有效更新字段（所有字段为NULL或未改变）" << std::endl;
            } else {
                for (const auto& [col, val] : updated_fields) {
                    std::cout << "  " << std::left << std::setw(20) << col
                              << " : " << val << std::endl;
                }
            }

            std::cout << "\n─── 执行SQL ───" << std::endl;
            std::cout << final_sql << "\n";

            // 6. 执行更新
            auto exec_start = std::chrono::steady_clock::now();
            sess.get() << final_sql;
            auto duration = std::chrono::steady_clock::now() - exec_start;

            // 7. 提交事务
            tr.commit();

            // 8. 打印结果
            std::cout << "─── 执行结果 ───" << std::endl;
            std::cout << "状态: 成功更新" << std::endl;
            std::cout << "耗时: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                      << "ms" << std::endl;
            std::cout << "总耗时: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start_time).count()
                      << "ms" << std::endl;

            return true;

        } catch(const std::exception& e) {
            tr.rollback();
            std::cerr << "\n!!!!! 更新失败 !!!!!" << std::endl;
            std::cerr << "错误类型: " << typeid(e).name() << std::endl;
            std::cerr << "错误信息: " << e.what() << std::endl;

            auto duration = std::chrono::steady_clock::now() - start_time;
            std::cerr << "总耗时: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                      << "ms" << std::endl;

            return false;
        }
    }

    /* 单条软删除（带完整日志）*/
    template<typename T>
    static bool remove(int id) {
        // 记录开始时间
        auto start_time = std::chrono::system_clock::now();
        std::cout << "[DELETE START] 开始删除操作 | 表: " << soci::table_info<T>::name
                  << " | ID: " << id << std::endl;

        Session sess;
        soci::transaction tr(sess.get());
        try {
            const auto& table = soci::table_info<T>::name;
            const auto& id_column = soci::table_info<T>::id_column();
            auto id_member = soci::table_info<T>::template id_member<T>();

            // 构建SQL
            std::string sql = std::string("UPDATE ") +
                              std::string(table) +
                              std::string(" SET is_delete = 1, updated_at = CURRENT_TIMESTAMP WHERE ") +
                              std::string(id_column) +
                              std::string(" = ");

            // 类型转换处理
            if constexpr (std::is_same_v<decltype(std::declval<T>().*id_member), std::optional<int>>) {
                sql += std::to_string(id);
                std::cout << "[DEBUG] 检测到optional<int>主键，已自动转换" << std::endl;
            } else {
                sql += std::to_string(id);
            }

            // 执行SQL前记录
            std::cout << "[SQL EXEC] 执行SQL: " << sql << std::endl;
            auto exec_start = std::chrono::system_clock::now();

            sess.get() << sql;
            tr.commit();

            // 记录执行时间
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - exec_start);
            std::cout << "[DELETE SUCCESS] 删除成功 | 耗时: " << duration.count() << "ms"
                      << " | 表: " << table << " | ID: " << id << std::endl;

            return true;
        } catch(const std::exception& e) {
            tr.rollback();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - start_time);

            std::cerr << "[DELETE FAILED] 删除失败 | 耗时: " << duration.count() << "ms" << std::endl;
            std::cerr << "错误详情: " << e.what() << std::endl;
            std::cerr << "表: " << soci::table_info<T>::name << " | ID: " << id << std::endl;
            return false;
        }
    }

/* 批量软删除（完全修正版）*/
    template<typename T>
    static bool bulkRemove(const std::vector<int>& ids) {
        auto start_time = std::chrono::system_clock::now();
        std::cout << "[BULK DELETE START] 开始批量删除 | 表: " << soci::table_info<T>::name
                  << " | 记录数: " << ids.size() << std::endl;

        if (ids.empty()) {
            std::cout << "[INFO] 空ID列表，跳过批量删除" << std::endl;
            return true;
        }

        Session sess;
        soci::transaction tr(sess.get());
        size_t last_processed = 0; // 用于记录最后处理的位置
        try {
            const auto& table = soci::table_info<T>::name;
            const auto& id_column = soci::table_info<T>::id_column();
            auto id_member = soci::table_info<T>::template id_member<T>();

            // 分批次处理
            const size_t BATCH_SIZE = 500;
            size_t total_affected = 0;

            for (size_t offset = 0; offset < ids.size(); offset += BATCH_SIZE) {
                last_processed = offset; // 更新最后处理位置
                auto end = std::min(offset + BATCH_SIZE, ids.size());
                auto batch_start = std::chrono::system_clock::now();

                // 安全构建SQL
                std::string sql = std::string("UPDATE ") +
                                  std::string(table) +
                                  std::string(" SET is_delete = 1, updated_at = CURRENT_TIMESTAMP WHERE ") +
                                  std::string(id_column) +
                                  std::string(" IN (");

                // 添加参数（使用标准问号占位符）
                for (size_t i = offset; i < end; ++i) {
                    sql += (i > offset ? "," : "") + std::to_string(ids[i]);
                }
                sql += ")";

                // 执行日志
                std::cout << "[BATCH EXEC] 批次: " << offset/BATCH_SIZE + 1
                          << " | 数量: " << end-offset << std::endl;
                std::cout << "[SQL EXEC] " << sql << std::endl;

                auto exec_start = std::chrono::system_clock::now();
                sess.get() << sql;

                // 批次日志
                auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now() - batch_start);
                std::cout << "[BATCH DONE] 影响行数: " << end-offset
                          << " | 耗时: " << batch_duration.count() << "ms" << std::endl;

                total_affected += end - offset;
            }

            tr.commit();

            // 汇总日志
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - start_time);
            std::cout << "[BULK DELETE SUCCESS] 批量删除完成 | 总影响行数: " << total_affected
                      << " | 总耗时: " << total_duration.count() << "ms" << std::endl;
            return true;
        } catch(const std::exception& e) {
            tr.rollback();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - start_time);

            std::cerr << "[BULK DELETE FAILED] 批量删除失败 | 已处理: "
                      << last_processed << "/" << ids.size() << std::endl;
            std::cerr << "错误详情: " << e.what() << std::endl;
            std::cerr << "表: " << soci::table_info<T>::name << std::endl;
            std::cerr << "总耗时: " << duration.count() << "ms" << std::endl;
            return false;
        }
    }

/* 恢复数据（带完整日志）*/
    template<typename T>
    static bool restore(int id) {
        auto start_time = std::chrono::system_clock::now();
        std::cout << "[RESTORE START] 开始恢复数据 | 表: " << soci::table_info<T>::name
                  << " | ID: " << id << std::endl;

        Session sess;
        soci::transaction tr(sess.get());
        try {
            const auto& table = soci::table_info<T>::name;
            const auto& id_column = soci::table_info<T>::id_column();
            auto id_member = soci::table_info<T>::template id_member<T>();

            // 构建SQL
            std::string sql = std::string("UPDATE ") +
                              std::string(table) +
                              std::string(" SET is_delete = 0, updated_at = CURRENT_TIMESTAMP WHERE ") +
                              std::string(id_column) +
                              std::string(" = ");

            // 类型处理
            if constexpr (std::is_same_v<decltype(std::declval<T>().*id_member), std::optional<int>>) {
                sql += std::to_string(id);
                std::cout << "[DEBUG] 检测到optional<int>主键，已自动转换" << std::endl;
            } else {
                sql += std::to_string(id);
            }

            // 执行前记录
            std::cout << "[SQL EXEC] 执行SQL: " << sql << std::endl;
            auto exec_start = std::chrono::system_clock::now();

            sess.get() << sql;
            tr.commit();

            // 记录结果
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - exec_start);
            std::cout << "[RESTORE SUCCESS] 恢复成功 | 耗时: " << duration.count() << "ms"
                      << " | 表: " << table << " | ID: " << id << std::endl;

            return true;
        } catch(const std::exception& e) {
            tr.rollback();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - start_time);

            std::cerr << "[RESTORE FAILED] 恢复失败 | 耗时: " << duration.count() << "ms" << std::endl;
            std::cerr << "错误详情: " << e.what() << std::endl;
            std::cerr << "表: " << soci::table_info<T>::name << " | ID: " << id << std::endl;
            return false;
        }
    }


    /* 批量恢复数据（带完整日志）*/
    template<typename T>
    static bool bulkRestore(const std::vector<int>& ids) {
        auto start_time = std::chrono::system_clock::now();
        std::cout << "[BULK RESTORE START] 开始批量恢复 | 表: " << soci::table_info<T>::name
                  << " | 记录数: " << ids.size() << std::endl;

        if (ids.empty()) {
            std::cout << "[INFO] 空ID列表，跳过批量恢复" << std::endl;
            return true;
        }

        Session sess;
        soci::transaction tr(sess.get());
        size_t last_processed = 0;
        try {
            const auto& table = soci::table_info<T>::name;
            const auto& id_column = soci::table_info<T>::id_column();
            auto id_member = soci::table_info<T>::template id_member<T>();

            // 分批次处理（每批500条）
            const size_t BATCH_SIZE = 500;
            size_t total_affected = 0;

            for (size_t offset = 0; offset < ids.size(); offset += BATCH_SIZE) {
                last_processed = offset;
                auto end = std::min(offset + BATCH_SIZE, ids.size());
                auto batch_start = std::chrono::system_clock::now();

                // 构建SQL
                std::ostringstream sql;
                sql << "UPDATE " << table
                    << " SET is_delete = 0, updated_at = CURRENT_TIMESTAMP WHERE "
                    << id_column << " IN (";

                // 添加ID列表
                for (size_t i = offset; i < end; ++i) {
                    sql << (i > offset ? "," : "") << ids[i];
                }
                sql << ")";

                // 执行日志
                std::cout << "\n[BATCH " << (offset/BATCH_SIZE + 1) << "]"
                          << " 处理ID范围: " << ids[offset] << "-" << ids[end-1]
                          << " (" << (end-offset) << "条)" << std::endl;
                std::cout << "[SQL] " << sql.str() << std::endl;

                // 执行批量恢复
                auto exec_start = std::chrono::system_clock::now();
                sess.get() << sql.str();
                auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now() - batch_start);

                std::cout << "[DONE] 影响行数: " << (end-offset)
                          << " | 耗时: " << batch_duration.count() << "ms" << std::endl;

                total_affected += end - offset;
            }

            tr.commit();

            // 汇总日志
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - start_time);
            std::cout << "\n[BULK RESTORE SUCCESS] 批量恢复完成"
                      << " | 成功恢复: " << total_affected << "/" << ids.size() << " 条"
                      << " | 总耗时: " << total_duration.count() << "ms" << std::endl;

            return true;
        } catch(const std::exception& e) {
            tr.rollback();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - start_time);

            std::cerr << "\n[BULK RESTORE FAILED] 批量恢复失败"
                      << " | 已处理: " << last_processed << "/" << ids.size() << std::endl;
            std::cerr << "错误详情: " << e.what() << std::endl;
            std::cerr << "表: " << soci::table_info<T>::name << std::endl;
            std::cerr << "总耗时: " << duration.count() << "ms" << std::endl;

            return false;
        }
    }


/* 完全修正的统一查询接口 */
    // 公共条件构建方法（返回SQL条件和参数）
    static std::tuple<std::string, std::vector<std::shared_ptr<std::string>>>
    buildQueryConditions(
            const std::vector<std::pair<std::string, std::string>>& conditions,
            const std::vector<std::pair<std::string, std::pair<std::string, std::string>>>& ranges,
            const std::vector<std::pair<std::string, std::string>>& fuzzyMatches,
            const std::vector<std::pair<std::string, std::vector<std::string>>>& inConditions,
            const std::string& rawWhere = "")
    {
        std::vector<std::string> whereClauses;
        auto params = std::make_shared<std::vector<std::shared_ptr<std::string>>>();

        auto addParam = [&](const std::string& value) {
            auto param = std::make_shared<std::string>(value);
            params->push_back(param);
            return "'" + *param + "'"; // 返回带引号的参数值
        };

        // 精确条件
        for (const auto& cond : conditions) {
            whereClauses.push_back(cond.first + " = " + addParam(cond.second));
        }
        // 范围条件
        for (const auto& range : ranges) {
            whereClauses.push_back(
                    range.first + " BETWEEN " +
                    addParam(range.second.first) + " AND " +
                    addParam(range.second.second)
            );
        }
        // 模糊条件
        for (const auto& fuzzy : fuzzyMatches) {
            whereClauses.push_back(fuzzy.first + " LIKE " + addParam("%" + fuzzy.second + "%"));
        }
        // IN条件
        for (const auto& inCond : inConditions) {
            if (!inCond.second.empty()) {
                std::vector<std::string> inValues;
                for (const auto& val : inCond.second) {
                    inValues.push_back(addParam(val));
                }
                whereClauses.push_back(
                        inCond.first + " IN (" + joinStrings(inValues, ",") + ")"
                );
            }
        }

        // 新增：处理自定义SQL条件
        if (!rawWhere.empty()) {
            whereClauses.push_back("(" + rawWhere + ")");
        }

        std::string whereClause;
        if (!whereClauses.empty()) {
            whereClause = " WHERE " + joinStrings(whereClauses, " AND ");
        }
        return {whereClause, *params};
    }

/* 统一查询方法 */
    /* 统一查询方法（复用query实现）*/
    template<typename T>
    static std::vector<T> queryAdvanced(
            const std::string& baseSql,
            const std::vector<std::pair<std::string, std::string>>& conditions = {},
            const std::vector<std::pair<std::string, std::pair<std::string, std::string>>>& ranges = {},
            const std::vector<std::pair<std::string, std::string>>& fuzzyMatches = {},
            const std::vector<std::pair<std::string, std::vector<std::string>>>& inConditions = {},
            const std::string& rawWhere = "", // 新增：直接插入的SQL条件
            const std::string& groupBy = "",
            const std::string& orderBy = "",
            int limit = -1,
            int offset = -1)
    {
        // 1. 构建SQL（保持原有逻辑）
        std::ostringstream sql;
        sql << baseSql;

        auto [whereClause, _] = buildQueryConditions(conditions, ranges, fuzzyMatches, inConditions,rawWhere);
        if (!whereClause.empty()) {
            sql << " " << whereClause;
        }

        if (!groupBy.empty()) {
            sql << " GROUP BY " << groupBy;
        }

        if (!orderBy.empty()) {
            sql << " ORDER BY " << orderBy;
        }

        if (limit > 0) {
            sql << " LIMIT " << limit;
        }
        if (offset > 0) {
            sql << " OFFSET " << offset;
        }

        // 2. 直接复用query方法
        return query<T>(sql.str());
    }

/* 分页查询方法（复用query实现）*/
    template<typename T>
    static PageResult<T> queryPage(
            const std::string& baseSql,
            int page,
            int pageSize,
            const std::vector<std::pair<std::string, std::string>>& conditions = {},
            const std::vector<std::pair<std::string, std::pair<std::string, std::string>>>& ranges = {},
            const std::vector<std::pair<std::string, std::string>>& fuzzyMatches = {},
            const std::vector<std::pair<std::string, std::vector<std::string>>>& inConditions = {},
            const std::string& rawWhere = "", // 新增：直接插入的SQL条件
            const std::string& groupBy = "",
            const std::string& orderBy = "")
    {
        PageResult<T> result;
        result.current_page = std::max(1, page);
        result.page_size = std::max(1, pageSize);

        // 1. 构建计数SQL
        std::ostringstream countSql;
        countSql << "SELECT COUNT(*) FROM (" << baseSql;

        auto [whereClause, _] = buildQueryConditions(conditions, ranges, fuzzyMatches, inConditions,rawWhere);
        if (!whereClause.empty()) {
            countSql << " " << whereClause;
        }

        if (!groupBy.empty()) {
            countSql << " GROUP BY " << groupBy;
        }
        countSql << ") AS tmp";

        // 2. 构建数据SQL
        std::ostringstream dataSql;
        dataSql << baseSql;

        if (!whereClause.empty()) {
            dataSql << " " << whereClause;
        }

        if (!groupBy.empty()) {
            dataSql << " GROUP BY " << groupBy;
        }

        if (!orderBy.empty()) {
            dataSql << " ORDER BY " << orderBy;
        } else {
            throw std::runtime_error("分页查询必须指定ORDER BY子句");
        }

        dataSql << " LIMIT " << result.page_size
                << " OFFSET " << ((result.current_page - 1) * result.page_size);

        // 3. 执行查询
        Session sess;
        try {
            // 计数查询
            sess.get() << countSql.str(), soci::into(result.total_items);

            // 数据查询（直接复用query方法）
            result.items = query<T>(dataSql.str());

            // 计算总页数
            result.total_pages = (result.total_items + result.page_size - 1) / result.page_size;
        } catch(const std::exception& e) {
            std::cerr << "[SQL ERROR] " << e.what() << "\n";
            std::cerr << "计数SQL: " << countSql.str() << "\n";
            std::cerr << "数据SQL: " << dataSql.str() << "\n";
            throw;
        }
        return result;
    }


    template<typename T>
    static std::vector<T> query_direct(const std::string& sql) {
        Session sess; // 复用已有的数据库会话管理类
        std::vector<T> results;

        try {
            // 1. 执行原始 SQL 查询
            soci::rowset<soci::row> rs = (sess.get().prepare << sql);

            // 2. 遍历结果集并映射到对象
            for (const soci::row& r : rs) {
                T obj;
                size_t col_idx = 0;

                // 3. 动态映射字段（需预定义 SOC_MAP 类型绑定）
                soci::table_info<T>::for_each_field([&](auto field_ptr, auto col_name) {
                    if (col_idx < r.size()) {
                        // 处理可选类型和时间戳特殊转换
                        if constexpr (is_optional_v<decltype(obj.*field_ptr)>) {
                            obj.*field_ptr = r.get<std::optional<
                                    typename decltype(obj.*field_ptr)::value_type>>(col_idx);
                        } else if constexpr (std::is_same_v<
                                std::decay_t<decltype(obj.*field_ptr)>,
                                std::chrono::system_clock::time_point>) {
                            obj.*field_ptr = std::chrono::system_clock::from_time_t(
                                    r.get<std::time_t>(col_idx));
                        } else {
                            obj.*field_ptr = r.get<std::decay_t<
                                    decltype(obj.*field_ptr)>>(col_idx);
                        }
                        col_idx++;
                    }
                });
                results.push_back(std::move(obj));
            }
        } catch (const std::exception& e) {
            std::cerr << "查询失败: " << e.what() << "\nSQL: " << sql << std::endl;
        }

        return results;
    }



    template<typename T>
    static std::vector<T> query(const std::string& sql) {
        const std::string operation = "SQL查询";
        auto start_time = std::chrono::steady_clock::now();

        try {
            Logger::Global().Debug("正在初始化数据库会话...");
            Session sess;

            // 检查会话连接状态
            if (!sess.get().is_connected()) {
                Logger::Global().Error("数据库会话连接失败");
                throw std::runtime_error("数据库连接异常");
            }
            Logger::Global().Debug("数据库会话初始化成功");

            std::vector<T> results;
            Logger::Global().Debug("准备执行SQL查询: " + sql);

            // 执行查询
            soci::rowset<soci::row> rs = (sess.get().prepare << sql);
            Logger::Global().Debug("SQL查询执行成功，开始处理结果集");

            size_t row_count = 0;
            for (const soci::row& r : rs) {
                Logger::Global().Debug("正在处理第 " + std::to_string(row_count + 1) + " 行数据，包含 " +
                                       std::to_string(r.size()) + " 个字段");

                try {
                    T obj;
                    soci::indicator ind;

                    Logger::Global().Debug("开始类型转换...");
                    type_conversion<T>::from_base(r, ind, obj);
                    Logger::Global().Debug("类型转换完成");

                    results.push_back(std::move(obj));
                    row_count++;

                    Logger::Global().Debug("成功添加对象到结果集，当前结果集大小: " +
                                           std::to_string(results.size()));
                } catch (const std::exception& e) {
                    Logger::Global().Error("处理第 " + std::to_string(row_count + 1) +
                                           " 行数据时出错: " + e.what());
                    continue;
                }
            }

            auto duration = std::chrono::steady_clock::now() - start_time;
            Logger::Global().Info(operation + "完成，返回 " + std::to_string(row_count) +
                                  " 条记录，耗时 " +
                                  std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) +
                                  " 毫秒");

            return results;

        } catch (const std::exception& e) {
            auto duration = std::chrono::steady_clock::now() - start_time;
            Logger::Global().Error(operation + "失败: " + e.what());
            Logger::Global().Debug("失败SQL: " + sql);
            Logger::Global().Debug("总耗时: " +
                                   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) +
                                   " 毫秒");
            throw;
        } catch (...) {
            Logger::Global().Error(operation + "失败: 未知异常");
            throw;
        }
    }



    template<typename T, typename... Args>
    static std::vector<T> query(const std::string& sql, Args&&... args) {
        Session sess;
        std::vector<T> results;

        try {
            // 1. 参数转换（使用与update相同的TypeConverter）
            auto convert_param = [](auto&& arg) -> std::string {
                using ParamType = std::decay_t<decltype(arg)>;
                return soci_helpers::TypeConverter<ParamType>::toSQL(arg);
            };

            // 2. 构建参数列表
            std::vector<std::string> param_strs = {
                    convert_param(std::forward<Args>(args))...
            };

            // 3. SQL构建（带参数替换）
            std::string final_sql = sql;
            size_t pos = 0;
            size_t param_index = 0;

            while ((pos = final_sql.find('?', pos)) != std::string::npos) {
                if (param_index >= param_strs.size()) {
                    throw std::runtime_error("参数数量不足");
                }
                final_sql.replace(pos, 1, param_strs[param_index++]);
                pos += param_strs[param_index-1].length();
            }

            // 4. 检查参数数量匹配
            if (param_index != param_strs.size()) {
                throw std::runtime_error("占位符数量不足");
            }

            // 5. 执行查询（日志与update风格一致）
            auto start_time = std::chrono::steady_clock::now();
            std::cout << "[SQL] " << final_sql << "\n";

            soci::rowset<soci::row> rs = (sess.get().prepare << final_sql);

            // 6. 结果处理（保持原有逻辑）
            size_t row_count = 0;
            for (const soci::row& r : rs) {
                try {
                    // 字段数量验证
                    size_t required_fields = 0;
                    soci::table_info<T>::for_each_field([&](auto&&...) { ++required_fields; });

                    if (r.size() < required_fields) {
                        std::cerr << "[WARN] 行 " << (row_count+1)
                                  << " 缺少字段 (实际 " << r.size()
                                  << ", 需要 " << required_fields << ")\n";
                        continue;
                    }

                    // 类型转换
                    T obj;
                    soci::indicator ind;
                    type_conversion<T>::from_base(r, ind, obj);
                    results.emplace_back(std::move(obj));
                    row_count++;

                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] 行 " << (row_count+1)
                              << " 转换失败: " << e.what() << "\n";
                    continue;
                }
            }

            // 7. 性能日志
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time);
            std::cout << "[STAT] 返回 " << row_count << " 行, 耗时 "
                      << duration.count() << "ms\n";

        } catch (const std::exception& e) {
            std::cerr << "[FAILED] " << e.what()
                      << "\n[原始SQL] " << sql << "\n";
            throw;
        }

        return results;
    }

    /**
    * 执行SQL查询并返回原始rowset
    * @param sql 完整的SQL查询语句
    * @return soci::rowset<soci::row> 可遍历的结果集
    */
    static soci::rowset<soci::row> queryRaw(const std::string& sql) {
        Session sess;
        return (sess.get().prepare << sql);
    }


    static nlohmann::json queryToJson(const std::string& sql) {
        Session sess;
        soci::rowset<soci::row> rs = (sess.get().prepare << sql);
        nlohmann::json results = nlohmann::json::array();

        for (const soci::row& row : rs) {
            nlohmann::json record;
            for (size_t i = 0; i < row.size(); ++i) {
                const auto& prop = row.get_properties(i);
                const soci::indicator ind = row.get_indicator(i);
                try {
                    if (ind == soci::i_null) {
                        record[prop.get_name()] = nullptr;
                        continue;
                    }
                    // 核心转换逻辑（一行搞定）
                    record[prop.get_name()] = [&]{
                        switch(prop.get_data_type()) {
                            case soci::dt_string:
                              return soci_helpers::TypeConverter<std::string>::to_json(row.get<std::string>(i));
                                break;
                            case soci::dt_integer:
                                return  soci_helpers::TypeConverter<int>::to_json(row.get<int>(i));
                                break;
                            case soci::dt_double:
                                return soci_helpers::TypeConverter<double>::to_json(row.get<double>(i));
                                break;
                            case soci::dt_date:
                                return soci_helpers::TypeConverter<std::tm>::to_json(row.get<std::tm>(i));
                                break;
                            default:
                                 "UNSUPPORTED_TYPE";
                        }
                    }();
                } catch (const std::exception& e) {
                    record[prop.get_name()] = "ERROR: " + std::string(e.what());
                }
            }
            results.push_back(std::move(record));
        }
        return results;
    }



// SQL值转义函数
    static std::string escape_sql_value(const std::string& value) {
        std::string escaped;
        escaped.reserve(value.length() * 1.1);
        for (char c : value) {
            if (c == '\'') escaped += '\'';
            escaped += c;
        }
        return escaped;
    }


private:
    static std::shared_ptr<soci::connection_pool>& getPool() {
        static std::shared_ptr<soci::connection_pool> pool;
        return pool;
    }


    template<typename... Args>
    static void buildInsertParams(std::string& columns, std::string& values, Args&&... args) {
        std::vector<std::string> colNames = {"column1", "column2"}; // 根据实际表结构调整
        columns = joinStrings(colNames, ", ");
        values = joinStrings(std::vector<std::string>(sizeof...(Args), "?"), ", ");
    }

    template<typename Tuple, std::size_t... I>
    static void applyTupleImpl(soci::statement& st, const Tuple& t, std::index_sequence<I...>) {
        (st, ..., (soci::use(std::get<I>(t))));
    }

    template<typename... Args>
    static void applyTupleToStatement(soci::statement& st, const std::tuple<Args...>& t) {
        applyTupleImpl(st, t, std::index_sequence_for<Args...>{});
    }

    static std::string joinStrings(const std::vector<std::string>& strs, const std::string& delim) {
        std::string result;
        for (size_t i = 0; i < strs.size(); ++i) {
            if (i != 0) result += delim;
            result += strs[i];
        }
        return result;
    }

    static void logError(const std::string& operation,
                         const std::string& sql,
                         const std::exception& e) {
        std::cerr << "[SociDB Error] " << operation << " failed: " << e.what()
                  << "\nSQL: " << sql << std::endl;
    }
};