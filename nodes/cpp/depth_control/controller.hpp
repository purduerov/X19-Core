#pragma once

#define MIN_OUTPUT -1.0
#define MAX_OUTPUT 1.0

typedef struct {
  double k_p;
  double k_i;
  double k_d;
} Weights;

class Controller {
public:
  // pid weights
  Weights weights;

  // depth target
  double target = 0;

  // pid values
  double integral = 0;
  double last_error = 0;

  // moving_average for filter
  double moving_avg[10] = {0};
  int mov_avg_index = 0; // index for queue

  int num_items = 0;

  Controller(Weights w) : weights(w) {};
  double compute(double measurement, double dt);
  double filter_data(double measurement);
};
