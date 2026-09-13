#pragma once
#include <thread>
#include <zmq.h>
#include <zmq.hpp>
#include <string>
#include <google/protobuf/message_lite.h>
#include <chrono>

namespace CppMsg{

constexpr auto THREAD_SLEEP = std::chrono::milliseconds(10);

template<typename MessageType>
class Subscriber{
    static_assert(std::is_base_of<google::protobuf::MessageLite, MessageType>::value, 
        "MessageType must inherit from google::protobuf::MessageLite");

    using CallbackType = void(*)(const MessageType &);

private:
    std::string address, topic;
    zmq::context_t context;
    zmq::socket_t socket;
    CallbackType callback;
    std::atomic<bool> keepRunning = true;
    

public:
    Subscriber(const std::string addressStr, const std::string topicStr, CallbackType callbackFn, bool bind = false) :
    address(std::move(addressStr)), topic(std::move(topicStr)), callback(callbackFn){
        context = zmq::context_t(1);
        socket = zmq::socket_t(context, zmq::socket_type::sub);
        if(bind){
            socket.bind(address);
        }else{
            socket.connect(address);
        }

        socket.set(zmq::sockopt::subscribe, topic);

    }

    bool spinOnce(std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{10}){
        zmq::pollitem_t items[] = {
            { static_cast<void*>(socket), 0, ZMQ_POLLIN, 0 }
        };

        zmq::poll(&items[0], 1, std::chrono::milliseconds(timeout_ms));

        if (items[0].revents & ZMQ_POLLIN) {
            try {
                zmq::message_t topicMsg;
                if (!socket.recv(topicMsg, zmq::recv_flags::none) || !topicMsg.more()) {
                    return false;
                }

                zmq::message_t payloadMsg;
                if (!socket.recv(payloadMsg, zmq::recv_flags::none)) {
                    return false;
                }

                MessageType message;
                if (message.ParseFromArray(payloadMsg.data(), payloadMsg.size())) {
                    callback(message);
                    return true;
                } else {
                    std::cerr << "Error parsing message on topic '" << topic << "'\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing message on topic '" << topic << "': " << e.what() << "\n";
            }
            
        }
        return false;
    }

    void spin(){
        while(true){
            zmq::message_t topicMsg;
            if (!socket.recv(topicMsg, zmq::recv_flags::none)) {
                continue;
            }

            zmq::message_t payloadMsg;
            if (!socket.recv(payloadMsg, zmq::recv_flags::none)) {
                continue;
            }

            MessageType message;
            if(message.ParseFromArray(payloadMsg.data(), payloadMsg.size())){
                callback(message);
            }
        }
    }

    void spinThreaded(){
        while(keepRunning){
            zmq::message_t topicMsg;
            if (!socket.recv(topicMsg, zmq::recv_flags::none)){
                std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_SLEEP));
                continue;
            }

            zmq::message_t payloadMsg;
            if (!socket.recv(payloadMsg, zmq::recv_flags::none)){
                continue;
            }

            MessageType message;
            if(message.ParseFromArray(payloadMsg.data(), payloadMsg.size())){
                callback(message);
            }
        }
    }

    bool startSpinThreaded(){
        std::thread subThread(this->spinThreaded());
        return subThread.joinable();
    }

    void close(){
        socket.close();
    }
};

}