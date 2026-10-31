/**
 * @file depth_hold_test.cpp
 * @brief Simulates a vertical ROV model to verify the depth hold / override behavior.
 *
 * Plant: unit mass, linear drag, constant buoyancy disturbance. Depth is +down.
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "control/depth_control.hpp"
#include "control/depth_hold.hpp"

static int failures = 0;
#define CHECK(cond, ...)                         \
  do {                                           \
    if (!(cond)) {                               \
      std::printf("FAIL: " __VA_ARGS__);         \
      std::printf("\n");                         \
      failures++;                                \
    } else {                                     \
      std::printf("ok:   " __VA_ARGS__);         \
      std::printf("\n");                         \
    }                                            \
  } while (0)

struct Plant {
  double depth = 2.0, vel = 0.0;
  double drag = 2.0, buoyancy = -0.15; // buoyancy pushes up (toward smaller depth)
  void step(double force, double dt) {
    double acc = force + buoyancy - drag * vel;
    vel += acc * dt;
    depth += vel * dt;
  }
};

int main() {
  const double dt = 0.02;
  Plant plant;
  DepthControl ctrl((Weights){.k_p = 1.5, .k_i = 0.4, .k_d = 1.5});
  DepthHold hold(ctrl);

  double t = 0;
  auto run = [&](double seconds, bool enabled, bool override_held, double pilot_force) {
    double last_out = 0;
    for (double end = t + seconds; t < end; t += dt) {
      ctrl.record_measurement(plant.depth);
      last_out = hold.update(enabled, override_held, dt);
      plant.step(last_out + (override_held ? pilot_force : 0.0), dt);
    }
    return last_out;
  };

  // 1. Switch on at 2.0 m: should latch there and hold against buoyancy.
  run(15, true, false, 0);
  CHECK(std::fabs(ctrl.target - 2.0) < 1e-9, "latched target = starting depth (%.3f)", ctrl.target);
  CHECK(std::fabs(plant.depth - 2.0) < 0.02, "holds 2.0 m against buoyancy (depth %.3f)", plant.depth);

  // 2. Button held: output must be exactly 0 and pilot dives.
  double out = run(0.5, true, true, 0.8);
  CHECK(out == 0.0 && !hold.active(), "controller off while button held (out=%.3f)", out);
  run(3.5, true, true, 0.8);
  double drift_depth = plant.depth;
  CHECK(drift_depth > 3.0, "pilot moved the vehicle (depth %.3f)", drift_depth);

  // 3. Release (vehicle still moving): target latches to current depth, first command is 0.
  ctrl.record_measurement(plant.depth);
  double filtered_at_release = ctrl.get_measurement();
  double first = hold.update(true, false, dt);
  CHECK(std::fabs(ctrl.target - filtered_at_release) < 1e-9,
        "target = current depth on release (%.3f)", ctrl.target);
  CHECK(std::fabs(first) < 1e-9, "no kick on release (first out=%.6f)", first);
  CHECK(ctrl.integral == 0.0 || std::fabs(ctrl.integral) < 1e-3, "integral cleared");
  plant.step(first, dt);
  double latched = ctrl.target;

  // 4. Settles at the *new* depth, not the old one.
  run(20, true, false, 0);
  CHECK(std::fabs(plant.depth - latched) < 0.03,
        "settles at new target %.3f m (depth %.3f)", latched, plant.depth);
  CHECK(std::fabs(plant.depth - 2.0) > 0.5, "did not snap back to the old 2.0 m");

  // 5. Master disable -> 0 output; re-enable relatches wherever we are.
  out = run(1, false, false, 0);
  CHECK(out == 0.0, "master disable gives 0 output");
  plant.depth += 0.5; // vehicle was shoved while disabled
  run(0.3, false, false, 0); // filter keeps tracking while off
  run(0.02, true, false, 0);
  CHECK(std::fabs(ctrl.target - plant.depth) < 0.05, "re-enable relatches (target %.3f, depth %.3f)",
        ctrl.target, plant.depth);

  // 6. Output saturates and stays bounded.
  Plant p2; DepthControl c2((Weights){.k_p = 50, .k_i = 5, .k_d = 0}); DepthHold h2(c2);
  c2.record_measurement(p2.depth);
  h2.update(true, false, dt);
  c2.target = 10.0; // force a big error
  double o = h2.update(true, false, dt);
  CHECK(o <= MAX_OUTPUT && o >= MIN_OUTPUT, "output clamped (%.3f)", o);

  std::printf("%s\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
  return failures ? 1 : 0;
}
