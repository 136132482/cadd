// LogFilter.h
#pragma once
#include <drogon/HttpFilter.h>
using namespace drogon;

class LogFilter : public HttpFilter<LogFilter, false> {
public:
    void doFilter(const HttpRequestPtr& req,
                  FilterCallback&& fcb,
                  FilterChainCallback&& fccb) override {
        LOG_INFO << "Request: " << req->path();
        return fccb();
    }
};