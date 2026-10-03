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

void queue_measurement(double measurement, float* data, int position) {
}

double Controller::filter_data(double measurement) {
  // enqueue measurement
  moving_avg[mov_avg_index] = measurement;
  mov_avg_index++; mov_avg_index %= 10;
  num_items = (num_items + 1 > 9) ? 9 : num_items + 1; 

  // take average value
  double sum = 0;
  for (int i = 0; i < num_items; i++) {
    sum += moving_avg[i];
  }
  return sum / (double)num_items;
}



