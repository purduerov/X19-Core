/**
 * @file hello_sub.cpp
 * @brief Example subscriber node receiving telemetry data over ZeroMQ.
 */

#include <iostream>
#include <csignal>
#include <chrono>
#include "messaging/subscriber.hpp"
#include "telemetry.pb.h"

/**
 * @brief Callback invoked upon receiving a valid SensorData message.
 * @param data Deserialized Protobuf message payload.
 */
void callback(const rov::telemetry::SensorData &data) {
    std::cout << "Received: " << data.depth() << "\n";
}

/**
 * @brief Entry point for the subscriber example node.
 * 
 * Connects to the telemetry publisher, registers the deserialization callback,
 * and enters the blocking message dispatch loop.
 * 
 * @return int Exit status code (0 on success).
 */
int main() {
    std::cout << "Sub launched\n";

    // Instantiate subscriber connected to TCP port 5555 listening on "telemetry".
    CppMsg::Subscriber<rov::telemetry::SensorData> sub("tcp://127.0.0.1:5555", "telemetry", callback);

    // Enter blocking loop to continuously receive and dispatch incoming messages.
    sub.spin();

    std::cout << "Done spinning\n";
    return 0;
}