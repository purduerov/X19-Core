#pragma once
#include <zmq.hpp>
#include <string>
#include <google/protobuf/message_lite.h>

namespace CppMsg{

class Publisher{
    std::string address, topic;
    zmq::context_t context;
    zmq::socket_t socket;
    zmq::message_t topicMsg;
public:
    Publisher(std::string addressStr, std::string topicStr, bool bind = true) : 
    address(std::move(addressStr)), topic(std::move(topicStr)), context(1), 
    socket(context, zmq::socket_type::pub){
        if(bind){
            socket.bind(address);
        }else{
            socket.connect(address);
        }
    }
    
    void publish(const google::protobuf::MessageLite &protoMessage){
        const size_t payloadSize = protoMessage.ByteSizeLong();

        zmq::message_t topicMsg(topic.data(), topic.size());

        zmq::message_t payloadMsg(payloadSize);
        protoMessage.SerializeToArray(payloadMsg.data(), static_cast<int>(payloadSize));

        socket.send(topicMsg, zmq::send_flags::sndmore);

        socket.send(payloadMsg, zmq::send_flags::none);
    }
    void close(){
        socket.close();
    }

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    Publisher(Publisher&&) noexcept = default;
    Publisher& operator=(Publisher&&) = default;

    ~Publisher() = default;
};

}