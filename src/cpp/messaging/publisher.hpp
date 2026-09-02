#pragma once
#include <type_traits>
#include <zmq.hpp>
#include <string>
#include <google/protobuf/message_lite.h>


template<typename MessageType>
class Publisher{
    static_assert(std::is_base_of<google::protobuf::MessageLite, MessageType>::value, 
        "MessageType must inherit from google::protobuf::MessageLite");

    std::string address, topic;
    zmq::context_t context;
    zmq::socket_t socket;
    zmq::message_t topicMsg;
public:
    Publisher(const std::string &addressStr, const std::string &topicStr, bool bind = true) : 
    address(std::move(addressStr)), topic(std::move(topicStr)){
        context = zmq::context_t(1);
        socket = zmq::socket_t(context, zmq::socket_type::sub);
        if(bind){
            socket.bind(address);
        }else{
            socket.connect(address);
        }

        zmq::message_t topic_msg(topic.data(), topic.size());
    }
    
    // Serialze and publish protobuf message
    void publish(const MessageType &protoMessage){
        // Serialize payload to a string
        std::string payload;
        protoMessage.SerializeToString(&payload);

        // Encode the messages in the zmq format
        zmq::message_t payloadMsg(payload.data(), payload.size());

        // Send topic message with sndmore flag
        socket.send(topicMsg, zmq::send_flags::sndmore);

        // Send payload message with none flag
        socket.send(payloadMsg, zmq::send_flags::none);
    }
    void close(){
        socket.close();
    }

    ~Publisher(){
        socket.close();
    }
};

