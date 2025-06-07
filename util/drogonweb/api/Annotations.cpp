//#pragma once
//#include <drogon/drogon.h>
//#include <atomic>
//#include <memory>
//#include <mutex>
//#include <vector>
//#include <utility>
//#include <string>
//
//namespace DrogonWeb {
//    namespace Internal {
//        static std::atomic_uint64_t g_routeCounter{0};
//
//        inline std::string GenerateRouteId() {
//            thread_local uintptr_t counter = 0;
//            return std::to_string(g_routeCounter.fetch_add(1)) +
//                   "_" + std::to_string(++counter) +
//                   "_" + std::to_string(reinterpret_cast<uintptr_t>(&counter));
//        }
//    }
//
//    class RouteManager {
//    public:
//        using HandlerType = std::function<void(const drogon::HttpRequestPtr&,
//                                               std::function<void(const drogon::HttpResponsePtr&)>&&)>;
//
//        struct RouteItem {
//            std::string path;
//            drogon::HttpMethod method;
//            std::shared_ptr<HandlerType> handler;
//            std::vector<std::string> filters;
//        };
//
//        static void AddRoute(const std::string& path,
//                             drogon::HttpMethod method,
//                             HandlerType&& handler,
//                             const std::vector<std::string>& filters) {  // 添加const
//            std::lock_guard<std::mutex> lock(mutex_());
//            routes_().emplace_back(RouteItem{
//                    path,
//                    method,
//                    std::make_shared<HandlerType>(std::move(handler)),
//                    filters  // 直接传递const引用
//            });
//        }
//
//        static void RegisterAll(const std::string& basePath,
//                                const std::vector<std::string>& filters) {
//            std::lock_guard<std::mutex> lock(mutex_());
//            for (auto& route : routes_()) {
//                drogon::app().registerHandler(
//                        basePath + route.path,
//                        [handler = route.handler](const drogon::HttpRequestPtr& req,
//                                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
//                            (*handler)(req, std::move(cb));
//                        },
//                        {route.method},
//                        joinFilters_(route.filters)
//                );
//            }
//        }
//
//    private:
//        static std::vector<RouteItem>& routes_() {
//            static std::vector<RouteItem> instance;
//            return instance;
//        }
//
//        static std::mutex& mutex_() {
//            static std::mutex instance;
//            return instance;
//        }
//
//        static std::string joinFilters_(const std::vector<std::string>& filters) {
//            std::string result;
//            for (size_t i = 0; i < filters.size(); ++i) {
//                if (i != 0) result += ",";
//                result += filters[i];
//            }
//            return result;
//        }
//    };
//}
//
//#define __DR_ROUTE_ID DrogonWeb::Internal::GenerateRouteId()
//
//#define WEB_GROUP(base_path, ...) \
//namespace { \
//const std::vector<std::string> __web_filters_##__DR_ROUTE_ID{__VA_ARGS__}; \
//const auto __web_register_##__DR_ROUTE_ID = []() -> int { \
//    DrogonWeb::RouteManager::RegisterAll(base_path, __web_filters_##__DR_ROUTE_ID); \
//    return 0; \
//}(); \
//} \
//static_assert(true, "")
//
//#define GET(path) \
//namespace { \
//const auto __web_handler_##__DR_ROUTE_ID = [](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr; \
//struct __web_reg_##__DR_ROUTE_ID { \
//    static void Register() { \
//        DrogonWeb::RouteManager::AddRoute( \
//            path, \
//            drogon::Get, \
//            [](const drogon::HttpRequestPtr& req, \
//               std::function<void(const drogon::HttpResponsePtr&)>&& cb) { \
//                cb(__web_handler_##__DR_ROUTE_ID(req)); \
//            }, \
//            __web_filters_##__DR_ROUTE_ID \
//        ); \
//    } \
//}; \
//const auto __web_reg_instance_##__DR_ROUTE_ID = []() -> int { \
//    __web_reg_##__DR_ROUTE_ID::Register(); \
//    return 0; \
//}(); \
//} \
//auto __web_handler_##__DR_ROUTE_ID = [](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr
//
//#define POST(path) \
//namespace { \
//const auto __web_handler_##__DR_ROUTE_ID = [](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr; \
//struct __web_reg_##__DR_ROUTE_ID { \
//    static void Register() { \
//        DrogonWeb::RouteManager::AddRoute( \
//            path, \
//            drogon::Post, \
//            [](const drogon::HttpRequestPtr& req, \
//               std::function<void(const drogon::HttpResponsePtr&)>&& cb) { \
//                cb(__web_handler_##__DR_ROUTE_ID(req)); \
//            }, \
//            __web_filters_##__DR_ROUTE_ID \
//        ); \
//    } \
//}; \
//const auto __web_reg_instance_##__DR_ROUTE_ID = []() -> int { \
//    __web_reg_##__DR_ROUTE_ID::Register(); \
//    return 0; \
//}(); \
//} \
//auto __web_handler_##__DR_ROUTE_ID = [](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr
//
//#define ASYNC_POST(path) \
//namespace { \
//const auto __web_handler_##__DR_ROUTE_ID = [](const drogon::HttpRequestPtr& req, \
//                                           std::function<void(const drogon::HttpResponsePtr&)>&& cb) -> void; \
//struct __web_reg_##__DR_ROUTE_ID { \
//    static void Register() { \
//        DrogonWeb::RouteManager::AddRoute( \
//            path, \
//            drogon::Post, \
//            __web_handler_##__DR_ROUTE_ID, \
//            __web_filters_##__DR_ROUTE_ID \
//        ); \
//    } \
//}; \
//const auto __web_reg_instance_##__DR_ROUTE_ID = []() -> int { \
//    __web_reg_##__DR_ROUTE_ID::Register(); \
//    return 0; \
//}(); \
//} \
//auto __web_handler_##__DR_ROUTE_ID = [](const drogon::HttpRequestPtr& req, \
//                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) -> void