#include "controller.hpp"

double Controller::compute(double measurement, double dt) {
  double error = target - measurement;
  double p = weights.k_p;

  integral += error * dt;
  double i = weights.k_i * integral;

  double derivative = (error - last_error) / dt;
  double d = weights.k_d * derivative;

  double output = p + i + d;

  if (output > MAX_OUTPUT) {
    output = MAX_OUTPUT;
    integral -= error * dt;
  } else if (output < MIN_OUTPUT) {
    output = MIN_OUTPUT;
    integral -= error * dt;
  }

  last_error = error;

  return output;
}
