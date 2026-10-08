/**
 * @file hello_pub.cpp
 * @brief Example publisher node transmitting mock telemetry data over ZeroMQ.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include "messaging/publisher.hpp"
#include "telemetry.pb.h"

/**
 * @brief Entry point for the publisher example node.
 * 
 * Initializes the publisher, allows time for subscriber discovery to avoid
 * dropped messages, publishes 10 sensor telemetry messages at 10 Hz, and closes.
 * 
 * @return int Exit status code (0 on success).
 */
int main() {
    std::cout << "Pub launched\n";

    // Initialize publisher bound to TCP port 5555 for the "telemetry" topic.
    CppMsg::Publisher pub("tcp://127.0.0.1:5555", "telemetry");

    // Sleep to allow ZeroMQ subscriber connections to establish (slow-joiner mitigation).
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Publish 10 telemetry frames at 100 ms intervals.
    for (int i = 0; i < 10; ++i) {
        rov::telemetry::SensorData msg;
        msg.set_depth(1.24);

        pub.publish(msg);
        std::cout << "Published: " << msg.depth() << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Brief pause to allow socket buffers to flush before teardown.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Explicitly close socket resources.
    pub.close();

    return 0;
}