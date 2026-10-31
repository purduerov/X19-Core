#pragma once

#define MIN_OUTPUT -1.0
#define MAX_OUTPUT 1.0

#define EMA_Alpha 0.5 // 0 < \alpha < 1

typedef struct {
  double k_p;
  double k_i;
  double k_d;
} Weights;

class DepthControl {
public:
  // pid weights
  Weights weights;

  // depth target
  double target = 0;

  // pid values
  double integral = 0;
  double last_error = 0;
  bool has_last_error = false; // false -> no derivative on the first sample
  double last_output = 0;

  // exponential moving average filter
  double moving_avg = 0;
  bool has_measurement = false; // false until the first sample seeds the filter

  int num_items = 0;

  DepthControl(Weights w) : weights(w) {};
  double compute(double measurement, double dt);
  double compute_filtered(double dt);
  void record_measurement(double measurement);
  double get_measurement();

  // clear integral + derivative history (call whenever the target jumps)
  void reset();
  bool ready() const { return has_measurement; }
};
