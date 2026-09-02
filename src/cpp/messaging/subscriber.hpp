#pragma once
#include <zmq.hpp>
#include <string>
#include <google/protobuf/message.h>

template<typename MessageType>
class Subscriber{
    static_assert(std::is_base_of<google::protobuf::MessageLite, MessageType>::value, 
        "MessageType must inherit from google::protobuf::MessageLite");

    using CallbackType = void(*)(const MessageType &);

    std::string address, topic;
    zmq::context_t context;
    zmq::socket_t socket;
    CallbackType callback;
     

public:
    Subscriber(const std::string &addressStr, const std::string &topicStr, CallbackType callbackFn, bool bind = false) :
    address(std::move(addressStr)), topic(std::move(topicStr)), callback(std::move(callbackFn)){
        context = zmq::context_t(1);
        socket = zmq::socket_t(context, zmq::socket_type::pub);
        if(bind){
            socket.bind(address);
        }else{
            socket.connect(address);
        }

        socket.set(zmq::sockopt::subscribe, topic);

    }
};