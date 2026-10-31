#include "depth_control.hpp"

double DepthControl::compute(double measurement, double dt) {
  if (dt <= 0.0) {
    return last_output; // bad timestep, hold the previous command
  }

  double error = target - measurement;

  double p = weights.k_p * error;

  double candidate_integral = integral + error * dt;
  double i = weights.k_i * candidate_integral;

  double d = 0;
  if (has_last_error) {
    d = weights.k_d * (error - last_error) / dt;
  }

  double output = p + i + d;

  bool sat_high = output > MAX_OUTPUT;
  bool sat_low = output < MIN_OUTPUT;
  if (sat_high) output = MAX_OUTPUT;
  if (sat_low) output = MIN_OUTPUT;

  // anti-windup: only keep integrating if it isn't pushing further into saturation
  if (!((sat_high && error > 0) || (sat_low && error < 0))) {
    integral = candidate_integral;
  }

  last_error = error;
  has_last_error = true;
  last_output = output;

  return output;
}

double DepthControl::compute_filtered(double dt) {
  return this->compute(moving_avg, dt);
}

void DepthControl::record_measurement(double measurement) {
  if (!has_measurement) {
    // seed the filter so we don't ramp up from 0 m
    moving_avg = measurement;
    has_measurement = true;
    return;
  }
  // s_t     = \alpha    * x_t         + (1 - \alpha)    * s_{t-1}
  moving_avg = EMA_Alpha * measurement + (1 - EMA_Alpha) * moving_avg;
}

double DepthControl::get_measurement() {
  return moving_avg;
}

void DepthControl::reset() {
  integral = 0;
  last_error = 0;
  has_last_error = false;
  last_output = 0;
}
