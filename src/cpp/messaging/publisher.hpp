#pragma once
#include <string>
#include <utility>
#include <zmq.hpp>
#include <google/protobuf/message_lite.h>

namespace CppMsg{

class Publisher{
private:
    std::string address;
    std::string topic;
    zmq::context_t context;
    zmq::socket_t socket;

public:
    Publisher(std::string addressStr, std::string topicStr, bool bind = true) :
        address(std::move(addressStr)),
        topic(std::move(topicStr)),
        context(1),
        socket(context, zmq::socket_type::pub) {
        if (bind) {
            socket.bind(address);
        } else {
            socket.connect(address);
        }
    }

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    Publisher(Publisher&&) noexcept = default;
    Publisher& operator=(Publisher&&) noexcept = default;

    ~Publisher() {
        close();
    }

    bool publish(const google::protobuf::MessageLite &protoMessage) {
        const size_t payloadSize = protoMessage.ByteSizeLong();

        zmq::message_t payloadMsg(payloadSize);
        if (!protoMessage.SerializeToArray(payloadMsg.data(), static_cast<int>(payloadSize))) {
            return false;
        }

        auto res1 = socket.send(zmq::buffer(topic), zmq::send_flags::sndmore);
        if (!res1) {
            return false;
        }

        auto res2 = socket.send(payloadMsg, zmq::send_flags::none);
        return res2.has_value();
    }

    void close() {
        if (static_cast<bool>(socket)) {
            socket.close();
        }
    }
};

}