#define MIN_OUTPUT -1.0
#define MAX_OUTPUT 1.0

typedef struct {
  double k_p;
  double k_i;
  double k_d;
} Weights;

class Controller {
  Weights weights;
  double target;
  double integral;
  double last_error;
  double compute(double measurement, double dt);
};
