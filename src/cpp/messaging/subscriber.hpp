#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <zmq.hpp>
#include <google/protobuf/message_lite.h>

namespace CppMsg{

template<typename MessageType>
class Subscriber{
    static_assert(std::is_base_of<google::protobuf::MessageLite, MessageType>::value, 
        "MessageType must inherit from google::protobuf::MessageLite");

    using CallbackType = std::function<void(const MessageType &)>;

private:
    std::string address;
    std::string topic;
    zmq::context_t context;
    zmq::socket_t socket;
    CallbackType callback;
    std::atomic<bool> keepRunning{false};
    std::thread workerThread;

    void drainMultipart() {
        while (socket.get(zmq::sockopt::rcvmore)) {
            zmq::message_t discard;
            if (!socket.recv(discard, zmq::recv_flags::none)) {
                break;
            }
        }
    }

public:
    Subscriber(std::string addressStr, std::string topicStr, CallbackType callbackFn, bool bind = false) :
        address(std::move(addressStr)),
        topic(std::move(topicStr)),
        callback(std::move(callbackFn)),
        context(1),
        socket(context, zmq::socket_type::sub) {
        if (bind) {
            socket.bind(address);
        } else {
            socket.connect(address);
        }

        socket.set(zmq::sockopt::subscribe, topic);
        socket.set(zmq::sockopt::rcvtimeo, 100);
    }

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
    Subscriber(Subscriber&&) = delete;
    Subscriber& operator=(Subscriber&&) = delete;

    ~Subscriber() {
        close();
    }

    bool spinOnce(std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{10}) {
        zmq::pollitem_t items[] = {
            { static_cast<void*>(socket), 0, ZMQ_POLLIN, 0 }
        };

        zmq::poll(&items[0], 1, timeout_ms);

        if (items[0].revents & ZMQ_POLLIN) {
            try {
                zmq::message_t topicMsg;
                if (!socket.recv(topicMsg, zmq::recv_flags::none)) {
                    return false;
                }
                if (!topicMsg.more()) {
                    return false;
                }

                zmq::message_t payloadMsg;
                if (!socket.recv(payloadMsg, zmq::recv_flags::none)) {
                    drainMultipart();
                    return false;
                }

                if (payloadMsg.more()) {
                    drainMultipart();
                }

                MessageType message;
                if (message.ParseFromArray(payloadMsg.data(), static_cast<int>(payloadMsg.size()))) {
                    callback(message);
                    return true;
                } else {
                    std::cerr << "Error parsing message on topic '" << topic << "'\n";
                }
            } catch (const std::exception& e) {
                drainMultipart();
                std::cerr << "Error parsing message on topic '" << topic << "': " << e.what() << "\n";
            }
        }
        return false;
    }

    void spin() {
        while (true) {
            zmq::message_t topicMsg;
            if (!socket.recv(topicMsg, zmq::recv_flags::none)) {
                continue;
            }
            if (!topicMsg.more()) {
                continue;
            }

            zmq::message_t payloadMsg;
            if (!socket.recv(payloadMsg, zmq::recv_flags::none)) {
                drainMultipart();
                continue;
            }

            if (payloadMsg.more()) {
                drainMultipart();
            }

            MessageType message;
            if (message.ParseFromArray(payloadMsg.data(), static_cast<int>(payloadMsg.size()))) {
                callback(message);
            }
        }
    }

    void spinThreaded() {
        while (keepRunning.load(std::memory_order_relaxed)) {
            zmq::message_t topicMsg;
            if (!socket.recv(topicMsg, zmq::recv_flags::none)) {
                continue;
            }
            if (!topicMsg.more()) {
                continue;
            }

            zmq::message_t payloadMsg;
            if (!socket.recv(payloadMsg, zmq::recv_flags::none)) {
                drainMultipart();
                continue;
            }

            if (payloadMsg.more()) {
                drainMultipart();
            }

            MessageType message;
            if (message.ParseFromArray(payloadMsg.data(), static_cast<int>(payloadMsg.size()))) {
                callback(message);
            }
        }
    }

    bool startSpinThreaded() {
        if (workerThread.joinable()) {
            return false;
        }
        keepRunning.store(true, std::memory_order_relaxed);
        workerThread = std::thread(&Subscriber::spinThreaded, this);
        return workerThread.joinable();
    }

    bool stopSpinThreaded() {
        keepRunning.store(false, std::memory_order_relaxed);
        if (workerThread.joinable()) {
            workerThread.join();
            return true;
        }
        return false;
    }

    void close() {
        stopSpinThreaded();
        socket.close();
    }
};

}