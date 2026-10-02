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

namespace CppMsg {

/**
 * @class Subscriber
 * @brief Subscribes to a ZeroMQ topic and deserializes incoming Protobuf messages.
 * @tparam MessageType Message type derived from google::protobuf::MessageLite.
 */
template<typename MessageType>
class Subscriber {
    static_assert(std::is_base_of<google::protobuf::MessageLite, MessageType>::value, 
        "MessageType must inherit from google::protobuf::MessageLite");

public:
    /**
     * @brief Callback function type invoked upon message receipt.
     */
    using CallbackType = std::function<void(const MessageType &)>;

private:
    std::string address;                  ///< ZeroMQ endpoint address.
    std::string topic;                    ///< Subscription topic prefix filter.
    zmq::context_t context;               ///< ZeroMQ context instance.
    zmq::socket_t socket;                 ///< Underlying ZeroMQ SUB socket.
    CallbackType callback;                ///< Invoked when a valid message is parsed.
    std::atomic<bool> keepRunning{false}; ///< Execution flag for background worker thread.
    std::thread workerThread;             ///< Background thread for asynchronous reception.

    /**
     * @brief Discards any trailing frames in a multipart message sequence.
     */
    void drainMultipart() {
        while (socket.get(zmq::sockopt::rcvmore)) {
            zmq::message_t discard;
            if (!socket.recv(discard, zmq::recv_flags::none)) {
                break;
            }
        }
    }

public:
    /**
     * @brief Constructs a Subscriber, applies topic subscription filter, and sets default timeouts.
     * @param addressStr Endpoint address string (e.g., "tcp://localhost:5555").
     * @param topicStr Topic filter prefix to subscribe to.
     * @param callbackFn Function invoked upon successful message receipt.
     * @param bind True to bind the socket; false to connect.
     */
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

    /// Deleted copy constructor.
    Subscriber(const Subscriber&) = delete;
    /// Deleted copy assignment operator.
    Subscriber& operator=(const Subscriber&) = delete;
    /// Deleted move constructor.
    Subscriber(Subscriber&&) = delete;
    /// Deleted move assignment operator.
    Subscriber& operator=(Subscriber&&) = delete;

    /**
     * @brief Destructor. Stops running threads and closes socket resources.
     */
    ~Subscriber() {
        close();
    }

    /**
     * @brief Polls the socket once for incoming messages within the given timeout.
     * @param timeout_ms Poll timeout duration in milliseconds. Defaults to 10 ms.
     * @return True if a message was successfully received and dispatched; false otherwise.
     */
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

    /**
     * @brief Enters a blocking receive loop on the current thread.
     */
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

    /**
     * @brief Receive loop executed on the dedicated background worker thread.
     */
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

    /**
     * @brief Launches the message processing loop on a background thread.
     * @return True if the worker thread started; false if a thread is already running.
     */
    bool startSpinThreaded() {
        if (workerThread.joinable()) {
            return false;
        }
        keepRunning.store(true, std::memory_order_relaxed);
        workerThread = std::thread(&Subscriber::spinThreaded, this);
        return workerThread.joinable();
    }

    /**
     * @brief Stops the background worker thread and joins it.
     * @return True if a running thread was joined; false if no thread was active.
     */
    bool stopSpinThreaded() {
        keepRunning.store(false, std::memory_order_relaxed);
        if (workerThread.joinable()) {
            workerThread.join();
            return true;
        }
        return false;
    }

    /**
     * @brief Stops background processing and closes the ZeroMQ socket.
     */
    void close() {
        stopSpinThreaded();
        socket.close();
    }
};

}