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
  Weights weights;
  double target = 0;
  double integral = 0;
  double last_error = 0;
  Controller(Weights w) : weights(w) {};
  double compute(double measurement, double dt);
};
