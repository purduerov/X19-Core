/*
 * Control node. Everything under src/cpp/control/ builds into this one program.
 *
 * Inputs:  ControllerCommand (joystick + depth hold switches), SensorData
 *          (depth + orientation), ControllerTuning (live PID gains)
 * Output:  ThrusterCommand -- one rigid transform (position + rotation),
 *          the only thing this node publishes, for the thruster mapping.
 */
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <chrono>
#include <csignal>
#include <optional>
#include <thread>

#include "depth_control.hpp"
#include "depth_hold.hpp"
#include "messaging/publisher.hpp"
#include "messaging/subscriber.hpp"
#include "telemetry.pb.h"
#include "thruster_control.hpp"

// TODO: match these to the real port map
static const char *COMMAND_ADDR = "tcp://127.0.0.1:5556"; // ControllerCommand + ControllerTuning in
static const char *SENSOR_ADDR = "tcp://127.0.0.1:5555";  // SensorData in
static const char *OUTPUT_ADDR = "tcp://127.0.0.1:5557";  // ThrusterCommand out (we bind this one)

// Depth is +down, but the vertical command is world +z = up. Flip if your
// thruster mapping uses the other convention.
static const double DESCEND_TO_WORLD_Z = -1.0;

// No joystick command for this long => output a neutral transform (deadman).
static const double COMMAND_TIMEOUT_S = 0.5;
// No depth reading for this long => depth hold drops out, pilot keeps vertical.
static const double SENSOR_TIMEOUT_S = 0.5;

static const auto LOOP_PERIOD = std::chrono::milliseconds(20); // 50 Hz

static volatile std::sig_atomic_t running = 1;
static void on_signal(int) { running = 0; }

using Clock = std::chrono::steady_clock;
static double seconds_between(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

// x/y/z -> translation; roll/pitch/yaw -> rotation (intrinsic Z-Y-X: yaw, then pitch, then roll)
static Eigen::Isometry3d to_isometry(const rov::telemetry::ControllerCommand &m) {
  Eigen::Isometry3d T = Eigen::Isometry3d::Identity();
  T.linear() = (Eigen::AngleAxisd(m.yaw(), Eigen::Vector3d::UnitZ()) *
                Eigen::AngleAxisd(m.pitch(), Eigen::Vector3d::UnitY()) *
                Eigen::AngleAxisd(m.roll(), Eigen::Vector3d::UnitX()))
                   .toRotationMatrix();
  T.translation() = Eigen::Vector3d(m.x(), m.y(), m.z());
  return T;
}

static void publish_transform(CppMsg::Publisher &pub, const Eigen::Isometry3d &T) {
  Eigen::Quaterniond q(T.rotation());
  q.normalize();

  rov::telemetry::ThrusterCommand msg;
  msg.set_timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
  auto *t = msg.mutable_translation();
  t->set_x(static_cast<float>(T.translation().x()));
  t->set_y(static_cast<float>(T.translation().y()));
  t->set_z(static_cast<float>(T.translation().z()));
  auto *r = msg.mutable_rotation();
  r->set_w(static_cast<float>(q.w()));
  r->set_x(static_cast<float>(q.x()));
  r->set_y(static_cast<float>(q.y()));
  r->set_z(static_cast<float>(q.z()));
  pub.publish(msg);
}

int main() {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  DepthControl controller((Weights){.k_p = 0, .k_i = 0, .k_d = 0}); // TODO: tune (live via ControllerTuning)
  DepthHold hold(controller);

  // latest inputs (callbacks run inside spinOnce, i.e. on this thread)
  Eigen::Isometry3d joystick = Eigen::Isometry3d::Identity();
  Eigen::AngleAxisd pose(Eigen::Quaterniond::Identity()); // assume level until the IMU says otherwise
  bool enable = false, override_held = false;
  auto last_command = Clock::time_point::min();
  auto last_sensor = Clock::time_point::min();

  CppMsg::Subscriber<rov::telemetry::ControllerCommand> cmd_sub(
      COMMAND_ADDR, "command", [&](const rov::telemetry::ControllerCommand &m) {
        joystick = to_isometry(m);
        enable = m.enable_depth_control();
        override_held = m.depth_override();
        last_command = Clock::now();
      });

  CppMsg::Subscriber<rov::telemetry::SensorData> sensor_sub(
      SENSOR_ADDR, "telemetry", [&](const rov::telemetry::SensorData &m) {
        controller.record_measurement(m.depth());
        if (m.has_orientation()) {
          const auto &o = m.orientation();
          Eigen::Quaterniond q(o.w(), o.x(), o.y(), o.z());
          if (q.norm() > 1e-6) pose = Eigen::AngleAxisd(q.normalized());
        }
        last_sensor = Clock::now();
      });

  // live PID tuning (same address as commands, own topic)
  CppMsg::Subscriber<rov::telemetry::ControllerTuning> tuning_sub(
      COMMAND_ADDR, "tuning", [&](const rov::telemetry::ControllerTuning &m) {
        if (m.enable_tuning()) {
          controller.weights = (Weights){.k_p = m.k_p(), .k_i = m.k_i(), .k_d = m.k_d()};
        }
      });

  CppMsg::Publisher out_pub(OUTPUT_ADDR, "thruster_command");

  auto last_tick = Clock::now();
  auto next_tick = last_tick;

  while (running) {
    next_tick += LOOP_PERIOD;

    // drain everything that arrived since last cycle
    while (cmd_sub.spinOnce(std::chrono::milliseconds(0))) {}
    while (sensor_sub.spinOnce(std::chrono::milliseconds(0))) {}
    while (tuning_sub.spinOnce(std::chrono::milliseconds(0))) {}

    auto now = Clock::now();
    double dt = seconds_between(last_tick, now);
    last_tick = now;

    bool command_fresh = last_command != Clock::time_point::min() &&
                         seconds_between(last_command, now) < COMMAND_TIMEOUT_S;
    bool sensor_fresh = last_sensor != Clock::time_point::min() &&
                        seconds_between(last_sensor, now) < SENSOR_TIMEOUT_S;

    // stale inputs => depth hold off; DepthHold relatches when they come back
    double depth_out = hold.update(enable && command_fresh && sensor_fresh, override_held, dt);

    Eigen::Isometry3d result = Eigen::Isometry3d::Identity(); // neutral
    if (command_fresh) {
      std::optional<float> vertical;
      if (hold.active()) vertical = static_cast<float>(depth_out * DESCEND_TO_WORLD_Z);
      result = genThrusterCommand(pose, joystick, vertical);
    }
    publish_transform(out_pub, result);

    std::this_thread::sleep_until(next_tick);
  }

  // leave the vehicle neutral on shutdown
  publish_transform(out_pub, Eigen::Isometry3d::Identity());
  return 0;
}
