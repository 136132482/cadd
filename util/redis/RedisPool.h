#pragma once
#include <hiredis/hiredis.h>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <functional>

class RedisUtils {
private:

    struct Impl {

        std::unordered_map<redisContext*, std::chrono::steady_clock::time_point> last_used_time;


        Impl(std::string host, int port, std::string password, int db, int pool_size)
                : host(std::move(host)), port(port), password(std::move(password)),
                  db(db), max_size(pool_size), active_count(0) {
            // 预热连接池（初始连接数为池大小的1/3）
            for (int i = 0; i < std::max(1, pool_size / 3); ++i) {
                if (auto conn = CreateConnection()) {
                    connections.push(conn);
                }
            }
        }

        ~Impl() {
            std::lock_guard<std::mutex> lock(mutex);
            while (!connections.empty()) {
                if (auto ctx = connections.front()) {
                    redisFree(ctx);
                }
                connections.pop();
            }
        }

        [[nodiscard]] redisContext* CreateConnection() const {
            auto ctx = redisConnect(host.c_str(), port);
            if (!ctx || ctx->err) {
                if (ctx) redisFree(ctx);
                return nullptr;
            }

            // 认证
            if (!password.empty()) {
                auto reply = (redisReply*)redisCommand(ctx, "AUTH %s", password.c_str());
                if (!reply || reply->type == REDIS_REPLY_ERROR) {
                    redisFree(ctx);
                    if (reply) freeReplyObject(reply);
                    return nullptr;
                }
                freeReplyObject(reply);
            }

            // 选择数据库
            if (db > 0) {
                auto reply = (redisReply*)redisCommand(ctx, "SELECT %d", db);
                if (!reply || reply->type == REDIS_REPLY_ERROR) {
                    redisFree(ctx);
                    if (reply) freeReplyObject(reply);
                    return nullptr;
                }
                freeReplyObject(reply);
            }

            // 健康检查
            auto ping_reply = (redisReply*)redisCommand(ctx, "PING");
            if (!ping_reply || strcmp(ping_reply->str, "PONG") != 0) {
                redisFree(ctx);
                if (ping_reply) freeReplyObject(ping_reply);
                return nullptr;
            }
            freeReplyObject(ping_reply);
            return ctx;
        }

        redisContext* Acquire() {
            std::unique_lock<std::mutex> lock(mutex);

            // 优先使用空闲连接
            if (!connections.empty()) {
                auto ctx = connections.front();
                connections.pop();
                last_used_time.erase(ctx); // 移出空闲记录
                return ctx;
            }

            // 未达上限则创建新连接
            if (active_count < max_size) {
                if (auto ctx = CreateConnection()) {
                    active_count++;
                    return ctx;
                }
            }

            return nullptr;
        }

        void Release(redisContext* ctx) {
            if (!ctx) return;
            std::lock_guard<std::mutex> lock(mutex);
            connections.push(ctx);
            last_used_time[ctx] = std::chrono::steady_clock::now(); // 记录释放时间
        }

        std::string host;
        int port;
        std::string password;
        int db;  // 新增数据库索引
        int max_size;
        std::atomic<int> active_count;
        std::queue<redisContext*> connections;
        mutable std::mutex mutex;
    };

    static std::unique_ptr<Impl> impl_;
    static std::once_flag init_flag_;

    template<typename Func>
    static bool ExecuteCommand(Func&& cmd) {
        if (!impl_) return false;
        auto ctx = impl_->Acquire();
        if (!ctx) return false;
        bool result = cmd(ctx);
        impl_->Release(ctx);
        return result;
    }

    template<typename Func>
    static auto ExecuteCommandWithReturn(Func&& cmd) -> decltype(cmd(nullptr)) {
        using ReturnType = decltype(cmd(nullptr));
        ReturnType default_return{};
        if (!impl_) return default_return;
        auto ctx = impl_->Acquire();
        if (!ctx) return default_return;
        auto result = cmd(ctx);
        impl_->Release(ctx);
        return result;
    }
public:

    static void ReleaseAllConnections() {
        if (!impl_) return;
        std::lock_guard<std::mutex> lock(impl_->mutex);
        while (!impl_->connections.empty()) {
            auto ctx = impl_->connections.front();
            if (ctx) redisFree(ctx);
            impl_->connections.pop();
        }
        impl_->active_count = 0;
    }

    // 释放空闲超时的连接
    static void ReleaseIdleConnections(int idle_timeout_sec = 120) {
        if (!impl_) return;

        std::lock_guard<std::mutex> lock(impl_->mutex);
        size_t original_size = impl_->connections.size();

        auto now = std::chrono::steady_clock::now();
        while (!impl_->connections.empty()) {
            auto ctx = impl_->connections.front();
            auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(
                    now - impl_->last_used_time[ctx]
            ).count();

            if (idle_time >= idle_timeout_sec) {
                redisFree(ctx);
                impl_->connections.pop();
                impl_->last_used_time.erase(ctx);
            } else {
                break; // 队列按时间排序，后续连接未超时
            }
        }
    }



    static void Initialize(
            const std::string& host = "127.0.0.1",
            int port = 6379,
            const std::string& password = "123456",
            int db = 0,
            int pool_size = 100
    ) {
        std::call_once(init_flag_, [&] {
            impl_ = std::make_unique<Impl>(host, port, password, db, pool_size);
        });
    }

    // 字符串操作
    static bool SetString(const std::string& key, const std::string& value, int ttl_seconds = 0) {
        return ExecuteCommand([&](redisContext* ctx) {
            redisReply* reply = ttl_seconds > 0 ?
                                (redisReply*)redisCommand(ctx, "SET %s %b EX %d", key.c_str(), value.data(), value.size(), ttl_seconds) :
                                (redisReply*)redisCommand(ctx, "SET %s %b", key.c_str(), value.data(), value.size());
            bool success = (reply && reply->type == REDIS_REPLY_STATUS);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

// 修改所有时间参数从 ttl_ms 改为 ttl_seconds
    static bool SetStringWithTTL(const std::string& key,
                                 const std::string& value,
                                 int ttl_seconds) {  // 改为秒
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(
                    ctx, "SET %s %b EX %d",  // 使用 EX 秒级过期
                    key.c_str(), value.data(), value.size(), ttl_seconds
            );
            bool success = (reply && reply->type == REDIS_REPLY_STATUS);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    static std::string GetString(const std::string& key) {
        std::string result;
        ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "GET %s", key.c_str());
            if (reply && reply->type == REDIS_REPLY_STRING) {
                result.assign(reply->str, reply->len);
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    // 哈希表操作
    static bool HSet(const std::string& key, const std::string& field, const std::string& value) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "HSET %s %s %b",
                                                   key.c_str(), field.c_str(), value.data(), value.size());
            bool success = (reply && reply->type != REDIS_REPLY_ERROR);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    static bool HSetWithTTL(const std::string& key,
                            const std::string& field,
                            const std::string& value,
                            int ttl_seconds) {  // 改为秒
        return ExecuteCommand([&](redisContext* ctx) {
            const char* lua_script =
                    "redis.call('HSET', KEYS[1], ARGV[2], ARGV[3])\n"
                    "return redis.call('EXPIRE', KEYS[1], ARGV[1])";  // 改为 EXPIRE
            auto reply = (redisReply*)redisCommand(
                    ctx,
                    "EVAL %s 1 %s %d %s %b",
                    lua_script,
                    key.c_str(),
                    ttl_seconds,  // 秒
                    field.c_str(),
                    value.data(),
                    value.size()
            );
            bool success = (reply && reply->integer == 1);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }


    static bool HMSet(const std::string& key,
                      const std::unordered_map<std::string, std::string>& fields) {
        return ExecuteCommand([&](redisContext* ctx) {
            // 构建命令字符串
            std::string cmd = "HMSET " + key;
            for (const auto& [field, value] : fields) {
                cmd += " " + field + " " + value;
            }

            // 使用 redisCommand 直接执行构建好的命令字符串
            auto reply = (redisReply*)redisCommand(ctx, cmd.c_str());
            bool success = (reply && reply->type == REDIS_REPLY_STATUS);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

// 带TTL的HMSet
    static bool HMSetWithTTL(const std::string& key,
                             const std::unordered_map<std::string, std::string>& fields,
                             int ttl_seconds) {
        return ExecuteCommand([&](redisContext* ctx) {
            // 构建Lua脚本
            const char* lua_script =
                    "for i=2,#ARGV,2 do\n"
                    "  redis.call('HSET', KEYS[1], ARGV[i], ARGV[i+1])\n"
                    "end\n"
                    "return redis.call('EXPIRE', KEYS[1], ARGV[1])";

            // 准备参数数组
            std::vector<const char*> argv;
            std::vector<size_t> argvlen;

            // 添加脚本和参数数量
            argv.push_back(lua_script);
            argvlen.push_back(strlen(lua_script));

            argv.push_back("1");  // KEYS数量
            argvlen.push_back(1);

            argv.push_back(key.c_str());  // KEY[1]
            argvlen.push_back(key.size());

            // 添加TTL参数
            std::string ttl_str = std::to_string(ttl_seconds);
            argv.push_back(ttl_str.c_str());  // ARGV[1]
            argvlen.push_back(ttl_str.size());

            // 添加字段和值
            for (const auto& [field, value] : fields) {
                argv.push_back(field.c_str());   // ARGV[2], ARGV[4]...
                argvlen.push_back(field.size());

                argv.push_back(value.c_str());   // ARGV[3], ARGV[5]...
                argvlen.push_back(value.size());
            }

            // 执行命令
            auto reply = (redisReply*)redisCommandArgv(
                    ctx,
                    static_cast<int>(argv.size()),
                    argv.data(),
                    argvlen.data()
            );

            bool success = (reply && reply->type != REDIS_REPLY_ERROR);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    static std::string HGet(const std::string& key, const std::string& field) {
        std::string result;
        ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "HGET %s %s", key.c_str(), field.c_str());
            if (reply && reply->type == REDIS_REPLY_STRING) {
                result.assign(reply->str, reply->len);
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    static std::unordered_map<std::string, std::string> HGetAll(const std::string& key) {
        std::unordered_map<std::string, std::string> result;
        ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "HGETALL %s", key.c_str());
            if (reply && reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; i += 2) {
                    result.emplace(
                            std::string(reply->element[i]->str, reply->element[i]->len),
                            std::string(reply->element[i+1]->str, reply->element[i+1]->len)
                    );
                }
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    // 列表操作
    static long LPush(const std::string& key, const std::string& value) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            auto reply = (redisReply*)redisCommand(ctx, "LPUSH %s %b", key.c_str(), value.data(), value.size());
            long result = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : -1;
            if (reply) freeReplyObject(reply);
            return result;
        });
    }


    // 带TTL的LPush
    static long LPushWithTTL(const std::string& key,
                             const std::string& value,
                             int ttl_seconds) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            auto reply = (redisReply*)redisCommand(ctx,
                                                   "EVAL %s 1 %s %d %b",
                                                   "local r = redis.call('LPUSH', KEYS[1], ARGV[2]) "
                                                   "redis.call('EXPIRE', KEYS[1], ARGV[1]) "
                                                   "return r",
                                                   key.c_str(), ttl_seconds, value.data(), value.size());

            long result = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : -1;
            if (reply) freeReplyObject(reply);
            return result;
        });
    }


    static long RPush(const std::string& key, const std::string& value) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            auto reply = (redisReply*)redisCommand(ctx, "RPUSH %s %b", key.c_str(), value.data(), value.size());
            long result = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : -1;
            if (reply) freeReplyObject(reply);
            return result;
        });
    }

    static std::string LPop(const std::string& key) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> std::string {
            auto reply = (redisReply*)redisCommand(ctx, "LPOP %s", key.c_str());
            std::string result;
            if (reply && reply->type == REDIS_REPLY_STRING) {
                result.assign(reply->str, reply->len);
            }
            if (reply) freeReplyObject(reply);
            return result;
        });
    }

    static std::string RPop(const std::string& key) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> std::string {
            auto reply = (redisReply*)redisCommand(ctx, "RPOP %s", key.c_str());
            std::string result;
            if (reply && reply->type == REDIS_REPLY_STRING) {
                result.assign(reply->str, reply->len);
            }
            if (reply) freeReplyObject(reply);
            return result;
        });
    }

    static std::vector<std::string> LRange(const std::string& key, long start = 0, long stop = -1) {
        std::vector<std::string> result;
        ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "LRANGE %s %ld %ld", key.c_str(), start, stop);
            if (reply && reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    // 集合操作
    static bool SAdd(const std::string& key, const std::string& member) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "SADD %s %b",
                                                   key.c_str(), member.data(), member.size());
            bool success = (reply && reply->type != REDIS_REPLY_ERROR);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 带TTL的SAdd
    static bool SAddWithTTL(const std::string& key,
                            const std::string& member,
                            int ttl_seconds) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx,
                                                   "EVAL %s 1 %s %d %b",
                                                   "redis.call('SADD', KEYS[1], ARGV[2]) "
                                                   "return redis.call('EXPIRE', KEYS[1], ARGV[1])",
                                                   key.c_str(), ttl_seconds, member.data(), member.size());

            bool success = (reply && reply->type != REDIS_REPLY_ERROR);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }


    static bool SIsMember(const std::string& key, const std::string& member) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> bool {
            auto reply = (redisReply*)redisCommand(ctx, "SISMEMBER %s %s", key.c_str(), member.c_str());
            bool is_member = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
            if (reply) freeReplyObject(reply);
            return is_member;
        });
    }

    static std::vector<std::string> SMembers(const std::string& key) {
        std::vector<std::string> result;
        ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "SMEMBERS %s", key.c_str());
            if (reply && reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    // 有序集合操作
    static bool ZAdd(const std::string& key, double score, const std::string& member) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "ZADD %s %f %b",
                                                   key.c_str(), score, member.data(), member.size());
            bool success = (reply && reply->type != REDIS_REPLY_ERROR);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 带TTL的ZAdd
    static bool ZAddWithTTL(const std::string& key,
                            double score,
                            const std::string& member,
                            int ttl_seconds) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx,
                                                   "EVAL %s 1 %s %d %f %b",
                                                   "redis.call('ZADD', KEYS[1], ARGV[2], ARGV[3]) "
                                                   "return redis.call('EXPIRE', KEYS[1], ARGV[1])",
                                                   key.c_str(), ttl_seconds, score, member.data(), member.size());

            bool success = (reply && reply->type != REDIS_REPLY_ERROR);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }


    static std::vector<std::string> ZRange(const std::string& key, long start = 0, long stop = -1) {
        std::vector<std::string> result;
        ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "ZRANGE %s %ld %ld", key.c_str(), start, stop);
            if (reply && reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    // 基础删除操作
    static bool Del(const std::string& key) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "DEL %s", key.c_str());
            bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 批量删除
    static long Del(const std::vector<std::string>& keys) {
        if (keys.empty()) return 0;

        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            std::string cmd = "DEL";
            for (const auto& key : keys) {
                cmd += " " + key;
            }

            auto reply = (redisReply*)redisCommand(ctx, cmd.c_str());
            long deleted = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : 0;
            if (reply) freeReplyObject(reply);
            return deleted;
        });
    }

    // 哈希表字段删除
    static bool HDel(const std::string& key, const std::string& field) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "HDEL %s %s", key.c_str(), field.c_str());
            bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 哈希表批量删除字段
    static long HDel(const std::string& key, const std::vector<std::string>& fields) {
        if (fields.empty()) return 0;

        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            std::string cmd = "HDEL " + key;
            for (const auto& field : fields) {
                cmd += " " + field;
            }

            auto reply = (redisReply*)redisCommand(ctx, cmd.c_str());
            long deleted = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : 0;
            if (reply) freeReplyObject(reply);
            return deleted;
        });
    }

    // 集合元素删除
    static bool SRem(const std::string& key, const std::string& member) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "SREM %s %s", key.c_str(), member.c_str());
            bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 有序集合删除成员
    static bool ZRem(const std::string& key, const std::string& member) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "ZREM %s %s", key.c_str(), member.c_str());
            bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 按分数范围删除有序集合元素
    static long ZRemRangeByScore(const std::string& key, double min, double max) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            auto reply = (redisReply*)redisCommand(ctx, "ZREMRANGEBYSCORE %s %f %f",
                                                   key.c_str(), min, max);
            long removed = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : 0;
            if (reply) freeReplyObject(reply);
            return removed;
        });
    }

    // 模糊匹配删除（慎用，性能影响大）
    static long DeletePattern(const std::string& pattern) {
        return ExecuteCommandWithReturn([&](redisContext *ctx) -> long {
            // 先获取匹配的keys
            auto reply = (redisReply *) redisCommand(ctx, "SCAN 0 MATCH %s COUNT 1000", pattern.c_str());
            if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2) {
                if (reply) freeReplyObject(reply);
                return 0;
            }

            std::vector<std::string> keys;
            auto keys_reply = reply->element[1];
            for (size_t i = 0; i < keys_reply->elements; ++i) {
                keys.emplace_back(keys_reply->element[i]->str, keys_reply->element[i]->len);
            }
            freeReplyObject(reply);

            // 批量删除
            if (keys.empty()) return 0;
            return Del(keys);
        });
    }



    // 键操作
    static bool SetTTL(const std::string& key, int ttl_seconds) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx, "EXPIRE %s %d", key.c_str(), ttl_seconds);
            bool success = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    static bool KeyExists(const std::string& key) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> bool {
            auto reply = (redisReply*)redisCommand(ctx, "EXISTS %s", key.c_str());
            bool exists = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
            if (reply) freeReplyObject(reply);
            return exists;
        });
    }

    static long GetTTL(const std::string& key) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long {
            auto reply = (redisReply*)redisCommand(ctx, "PTTL %s", key.c_str());
            long ttl = -3;
            if (reply && reply->type == REDIS_REPLY_INTEGER) {
                ttl = reply->integer;
            }
            if (reply) freeReplyObject(reply);
            return ttl;
        });
    }

    static std::unordered_map<std::string, bool> KeysExist(const std::vector<std::string>& keys) {
        std::unordered_map<std::string, bool> result;
        if (keys.empty()) return result;

        ExecuteCommand([&](redisContext* ctx) {
            std::string cmd = "EXISTS";
            for (const auto& key : keys) {
                cmd += " " + key;
                result[key] = false;
            }

            auto reply = (redisReply*)redisCommand(ctx, cmd.c_str());
            if (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0) {
                for (auto& kv : result) {
                    kv.second = true;
                }
            }
            if (reply) freeReplyObject(reply);
            return true;
        });
        return result;
    }

    // 原子操作
    static long long AtomicIncr(const std::string& key, long long value = 1) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> long long {
            auto reply = (redisReply*)redisCommand(ctx, "INCRBY %s %lld", key.c_str(), value);
            long long result = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : -1;
            if (reply) freeReplyObject(reply);
            return result;
        });
    }

    static bool AtomicCAS(const std::string& key, const std::string& expected, const std::string& new_value) {
        return ExecuteCommand([&](redisContext* ctx) {
            auto reply = (redisReply*)redisCommand(ctx,
                                                   "EVAL %s 1 %s %s %s",
                                                   "if redis.call('GET', KEYS[1]) == ARGV[1] then "
                                                   "return redis.call('SET', KEYS[1], ARGV[2]) "
                                                   "else return 0 end",
                                                   key.c_str(), expected.c_str(), new_value.c_str());

            bool success = (reply && reply->type == REDIS_REPLY_STATUS);
            if (reply) freeReplyObject(reply);
            return success;
        });
    }

    // 在 RedisUtils 类的 public 部分添加这个方法
    static std::vector<std::string> Keys(const std::string& pattern) {
        return ExecuteCommandWithReturn([&](redisContext* ctx) -> std::vector<std::string> {
            std::vector<std::string> result;

            // 使用 SCAN 命令替代 KEYS 命令（生产环境推荐）
            int cursor = 0;
            do {
                auto reply = (redisReply*)redisCommand(ctx, "SCAN %d MATCH %s COUNT 1000",
                                                       cursor, pattern.c_str());

                if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2) {
                    if (reply) freeReplyObject(reply);
                    break;
                }

                // 解析新的游标位置
                cursor = atoi(reply->element[0]->str);

                // 收集匹配的keys
                auto keys_reply = reply->element[1];
                for (size_t i = 0; i < keys_reply->elements; ++i) {
                    result.emplace_back(keys_reply->element[i]->str,
                                        keys_reply->element[i]->len);
                }

                freeReplyObject(reply);
            } while (cursor != 0); // 直到迭代完成

            return result;
        });
    }

    // 分布式锁
    class Lock {
    public:
        Lock(const std::string& key, int ttl_ms = 5000)
                : key_(key), ttl_(ttl_ms), locked_(false) {}

        bool TryLock() {
            return ExecuteCommand([this](redisContext* ctx) {
                auto reply = (redisReply*)redisCommand(ctx,
                                                       "SET %s %s NX PX %d", key_.c_str(), "locked", ttl_);
                locked_ = (reply && reply->type == REDIS_REPLY_STATUS);
                if (reply) freeReplyObject(reply);
                return locked_;
            });
        }

        // 新增续期功能
        bool RenewLock(int additional_seconds) {
            if (!locked_) return false;
            return ExecuteCommand([this, additional_seconds](redisContext* ctx) {
                auto reply = (redisReply*)redisCommand(ctx,
                                                       "PEXPIRE %s %d",
                                                       key_.c_str(),
                                                       ttl_ + additional_seconds * 1000);
                bool success = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
                if (reply) freeReplyObject(reply);
                return success;
            });
        }

        void Unlock() {
            if (locked_) {
                ExecuteCommand([this](redisContext* ctx) {
                    auto reply = (redisReply*)redisCommand(ctx, "DEL %s", key_.c_str());
                    if (reply) freeReplyObject(reply);
                    locked_ = false;
                    return true;
                });
            }
        }

        ~Lock() { Unlock(); }

    private:
        std::string key_;
        int ttl_;
        bool locked_;
    };
};

// 静态成员初始化
std::unique_ptr<RedisUtils::Impl> RedisUtils::impl_;
std::once_flag RedisUtils::init_flag_;