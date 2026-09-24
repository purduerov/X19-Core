#include <iostream>
#include <csignal>
#include <chrono>
#include "subscriber.hpp"
#include "telemetry.pb.h"


void callback(const rov::telemetry::SensorData &data){
    std::cout << "Received: " << data.depth() << "\n";
}

int main(){

    std::cout << "Sub launched\n";
    CppMsg::Subscriber<rov::telemetry::SensorData> sub("tcp://127.0.0.1:5555", "telemetry", callback);

    sub.spin();

    std::cout << "Done spinning\n";
    return 0;
}