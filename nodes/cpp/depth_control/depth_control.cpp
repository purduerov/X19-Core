#include "controller.hpp"

double check_enabled() {
  return 1;
}

double read_target() {
  return 10;
}

double read_depth() {
  return 5;
}

void send_thruster_power(double value);

int main() {
  Controller *controller = new Controller((Weights) {.k_p = 0, .k_i = 0, .k_d = 0});

  while (check_enabled()) {
    // recieve protobuf message
    double target = read_target();

    // recieve sensor data
    double measurement = read_depth();

    // get change in time
    double dt = 0.1;

    send_thruster_power(controller->compute(measurement, dt));
  }
}
