#include <subscriber.hpp>
#include <telemetry.pb.h>
#include <iostream>

void callback(const rov::telemetry::test &msg){
    std::cout << msg.msg() << "\n";
}

int main(){
    auto sub = CppMsg::Subscriber<rov::telemetry::test>("tcp://*:5556", "test", callback);
    sub.spin();
}