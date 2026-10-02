#pragma once
#include <string>
#include <utility>
#include <zmq.hpp>
#include <google/protobuf/message_lite.h>

/**
 * @namespace CppMsg
 * @brief Messaging utilities wrapping ZeroMQ and Google Protocol Buffers.
 */
namespace CppMsg {

/**
 * @class Publisher
 * @brief Publishes serialized Protocol Buffer messages over a ZeroMQ PUB socket.
 */
class Publisher {
private:
    std::string address;    ///< ZeroMQ endpoint address.
    std::string topic;      ///< Message topic prefix.
    zmq::context_t context; ///< ZeroMQ context instance.
    zmq::socket_t socket;   ///< Underlying ZeroMQ PUB socket.

public:
    /**
     * @brief Constructs a Publisher and sets up the underlying socket.
     * @param addressStr Endpoint address string (e.g., "tcp://*:5555").
     * @param topicStr Topic string prefixed to outgoing messages.
     * @param bind True to bind the socket; false to connect.
     */
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

    /// Deleted copy constructor.
    Publisher(const Publisher&) = delete;
    /// Deleted copy assignment operator.
    Publisher& operator=(const Publisher&) = delete;

    /// Default move constructor.
    Publisher(Publisher&&) noexcept = default;
    /// Default move assignment operator.
    Publisher& operator=(Publisher&&) noexcept = default;

    /**
     * @brief Destructor. Closes the socket connection.
     */
    ~Publisher() {
        close();
    }

    /**
     * @brief Serializes and publishes a Protocol Buffer message.
     * @param protoMessage Protobuf message instance to serialize and send.
     * @return True if both topic frame and payload were sent successfully; false otherwise.
     */
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

    /**
     * @brief Closes the ZeroMQ socket if currently active.
     */
    void close() {
        if (static_cast<bool>(socket)) {
            socket.close();
        }
    }
};

}