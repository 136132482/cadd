#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <memory>
#include <type_traits>

template <typename T>
class PgModel {
public:
    // 初始化（表名和主键）
    explicit PgModel(std::shared_ptr<drogon::orm::DbClient> db,
                     const std::string& table,
                     const std::string& pk = "id")
            : db_(db), table_(table), pk_(pk) {}

    // ================ 查询构建器 ================
    class QueryBuilder {
    public:
        QueryBuilder(std::shared_ptr<drogon::orm::DbClient> db,
                     const std::string& table)
                : db_(db), table_(table) {}

        // 精确查询（=）
        QueryBuilder& where(const std::string& field, const auto& value) {
            conditions_.emplace_back(field + " = $" + std::to_string(++param_count_));
            params_.push_back(value);
            return *this;
        }

        // 模糊查询（LIKE）
        QueryBuilder& like(const std::string& field, const std::string& pattern) {
            conditions_.emplace_back(field + " LIKE $" + std::to_string(++param_count_));
            params_.push_back("%" + pattern + "%");
            return *this;
        }

        // 范围查询
        QueryBuilder& between(const std::string& field, const auto& from, const auto& to) {
            conditions_.emplace_back(field + " BETWEEN $" + std::to_string(++param_count_) +
                                     " AND $" + std::to_string(++param_count_));
            params_.push_back(from);
            params_.push_back(to);
            return *this;
        }

        // 嵌套查询（子查询）
        template <typename SubQuery>
        QueryBuilder& in(const std::string& field, SubQuery&& subquery) {
            conditions_.emplace_back(field + " IN (" + subquery.build() + ")");
            auto subParams = subquery.getParams();
            params_.insert(params_.end(), subParams.begin(), subParams.end());
            return *this;
        }

        // 排序
        QueryBuilder& orderBy(const std::string& field, bool asc = true) {
            order_ = " ORDER BY " + field + (asc ? " ASC" : " DESC");
            return *this;
        }

        // 分页
        QueryBuilder& paginate(int page, int perPage = 10) {
            limit_ = " LIMIT " + std::to_string(perPage) +
                     " OFFSET " + std::to_string((page - 1) * perPage);
            return *this;
        }

        // 执行查询（返回模型列表）
        void find(std::function<void(const std::vector<T>&)> callback) {
            std::string sql = "SELECT * FROM " + table_;
            if (!conditions_.empty()) {
                sql += " WHERE " + joinConditions();
            }
            sql += order_ + limit_;

            db_->execSqlAsync(
                    sql, params_,
                    [callback](const drogon::orm::Result& r) {
                        std::vector<T> result;
                        for (const auto& row : r) {
                            result.emplace_back(row.as<T>());
                        }
                        callback(result);
                    },
                    [](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "Query error: " << e.base().what();
                    });
        }

        // 获取单条记录
        void first(std::function<void(const std::optional<T>&)> callback) {
            limit_ = " LIMIT 1";
            find([callback](const std::vector<T>& result) {
                callback(result.empty() ? std::nullopt : std::make_optional(result.front()));
            });
        }

    private:
        std::string joinConditions() const {
            return std::accumulate(
                    std::next(conditions_.begin()), conditions_.end(),
                    conditions_.front(),
                    [](std::string a, const std::string& b) {
                        return a + " AND " + b;
                    });
        }

        std::shared_ptr<drogon::orm::DbClient> db_;
        std::string table_;
        std::vector<std::string> conditions_;
        std::vector<drogon::orm::Argument> params_;
        std::string order_;
        std::string limit_;
        int param_count_ = 0;
    };

    // ================ CRUD 操作 ================
    // 创建记录
    template <typename Dto>
    void create(const Dto& data, std::function<void(const T&)> callback) {
        auto fields = getFields(data);
        auto placeholders = getPlaceholders(fields.size());

        db_->execSqlAsync(
                "INSERT INTO " + table_ + " (" + joinFields(fields) + ") " +
                "VALUES (" + placeholders + ") RETURNING *",
                getValues(data, fields),
                [callback](const drogon::orm::Result& r) {
                    callback(r.front().as<T>());
                },
                [](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "Create error: " << e.base().what();
                });
    }

    // 更新记录
    template <typename Dto>
    void update(int id, const Dto& data, std::function<void(const T&)> callback) {
        auto fields = getFields(data);
        auto setClause = getSetClause(fields);

        db_->execSqlAsync(
                "UPDATE " + table_ + " SET " + setClause +
                " WHERE " + pk_ + " = $" + std::to_string(fields.size() + 1) + " RETURNING *",
                [&]() {
                    auto values = getValues(data, fields);
                    values.push_back(id);
                    return values;
                }(),
                [callback](const drogon::orm::Result& r) {
                    callback(r.front().as<T>());
                },
                [](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "Update error: " << e.base().what();
                });
    }

    // 删除记录
    void remove(int id, std::function<void(bool)> callback) {
        db_->execSqlAsync(
                "DELETE FROM " + table_ + " WHERE " + pk_ + " = $1",
                {id},
                [callback](const drogon::orm::Result& r) {
                    callback(r.affectedRows() > 0);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "Delete error: " << e.base().what();
                    callback(false);
                });
    }

    // 开启查询构建
    QueryBuilder query() {
        return QueryBuilder(db_, table_);
    }

private:
    // 辅助函数
    std::vector<std::string> getFields(const auto& dto) {
        std::vector<std::string> fields;
        for (const auto& [key, value] : dto) {
            if (!value.isNull()) {
                fields.push_back(key);
            }
        }
        return fields;
    }

    std::string joinFields(const std::vector<std::string>& fields) {
        return std::accumulate(
                std::next(fields.begin()), fields.end(),
                fields.front(),
                [](std::string a, const std::string& b) {
                    return a + ", " + b;
                });
    }

    std::string getPlaceholders(int count) {
        std::string result;
        for (int i = 1; i <= count; ++i) {
            result += (i == 1 ? "$" : ", $") + std::to_string(i);
        }
        return result;
    }

    std::string getSetClause(const std::vector<std::string>& fields) {
        std::string result;
        for (size_t i = 0; i < fields.size(); ++i) {
            result += (i == 0 ? "" : ", ") + fields[i] + " = $" + std::to_string(i + 1);
        }
        return result;
    }

    std::vector<drogon::orm::Argument> getValues(const auto& dto,
                                                 const std::vector<std::string>& fields) {
        std::vector<drogon::orm::Argument> values;
        for (const auto& field : fields) {
            values.push_back(dto[field]);
        }
        return values;
    }

    std::shared_ptr<drogon::orm::DbClient> db_;
    std::string table_;
    std::string pk_;
};