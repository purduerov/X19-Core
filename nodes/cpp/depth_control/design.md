# core + surface layout
- surface -> core
  - `bool` enable/disable depth control
  - `bool` enable/disable testing mode
  - `float` target value
  - *while testing* {`float`, `float`, `float`} k_p, k_i, k_d, weight values
- core -> surface
  - *to thrusters* `float` calculated thruster output value
  - `float` measured depth value
  - *while testing* {`float`, `float`, `float`} initial weights
