#include <iostream>
#include <chrono>
#include <thread>
#include "publisher.hpp"
#include "telemetry.pb.h"

int main(){
    std::cout << "Pub launched\n";
    CppMsg::Publisher pub("tcp://127.0.0.1:5555", "telemetry");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for(int i = 0; i < 10; ++i){
        rov::telemetry::SensorData msg;
        msg.set_depth(1.24);
        pub.publish(msg);
        std::cout << "Published: " << msg.depth() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pub.close();
    return 0;
}