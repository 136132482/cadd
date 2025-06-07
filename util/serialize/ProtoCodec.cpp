// proto_codec.hpp
#pragma once
#include <google/protobuf/message.h>
#include <string>

class ProtoCodec {
public:
    // 序列化为字符串
    static std::string encode(const google::protobuf::Message& msg) {
        std::string output;
        if (!msg.SerializeToString(&output)) {
            throw std::runtime_error("Protobuf serialization failed");
        }
        return output;
    }

    // 从字符串反序列化
    template <typename T>
    static T decode(const std::string& data) {
        T msg;
        if (!msg.ParseFromString(data)) {
            throw std::runtime_error("Protobuf deserialization failed");
        }
        return msg;
    }

    // ZeroMQ消息快速转换
    template <typename T>
    static zmq::message_t to_zmq_msg(const T& proto_msg) {
        std::string data = encode(proto_msg);
        return zmq::message_t(data.begin(), data.end());
    }
};