#pragma once

#include "depth_control.hpp"

/*
 * Depth hold with a "free depth" override button.
 *
 *   active = enabled && !override_held && controller has a depth reading
 *
 *   - While active, the PID drives the vertical axis toward the target.
 *   - While inactive, the output is 0 and the pilot has the vertical axis.
 *   - On every inactive -> active transition (button released, or depth
 *     control first switched on) the target is latched to the current
 *     filtered depth and the PID state is cleared, so the first command
 *     after release is exactly 0 (no kick from a stale integral/error).
 *
 * Call controller.record_measurement() every cycle *before* update() so the
 * filtered depth keeps tracking while the controller is off.
 *
 * Output is "descend-positive" (depth increases downward), same as the
 * controller. Convert to your world-z convention at the call site.
 */
class DepthHold {
public:
  explicit DepthHold(DepthControl &c) : ctrl(c) {}

  double update(bool enabled, bool override_held, double dt) {
    bool now_active = enabled && !override_held && ctrl.ready();

    if (now_active && !was_active) {
      ctrl.target = ctrl.get_measurement();
      ctrl.reset();
    }
    was_active = now_active;

    return now_active ? ctrl.compute_filtered(dt) : 0.0;
  }

  bool active() const { return was_active; }
  double target() const { return ctrl.target; }

private:
  DepthControl &ctrl;
  bool was_active = false;
};
