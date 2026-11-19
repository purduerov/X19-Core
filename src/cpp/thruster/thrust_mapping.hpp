#pragma once

#include <Eigen/Dense>
#include <array>
#include <cstdint>

inline constexpr double SCALE = 0.0254;
inline constexpr double THRUST_MAX = 3.71;
inline constexpr double THRUST_MIN = -2.92;
inline constexpr double Z_OFFSET = 0.0;

class ThrustMapper {
public:
    static constexpr int NUM_THRUSTERS = 6;
    using Effort = Eigen::Matrix<double, 6, 1>;
    using Thrust = Eigen::Matrix<double, NUM_THRUSTERS, 1>;
    using ThrustMap = Eigen::Matrix<double, NUM_THRUSTERS, 6>;
    using Pwm = std::array<std::uint8_t, NUM_THRUSTERS>;

    ThrustMapper();
    ThrustMap Setup();
    Thrust GetThrust(const Effort& effort) const;
    Pwm GetPwm(const Effort& effort) const;
    static Pwm ThrustToPwm(const Thrust& thrust);

    std::array<int, NUM_THRUSTERS> invert_thrust_;
    std::array<Eigen::Vector3d, NUM_THRUSTERS> position_vectors_;
    std::array<Eigen::Vector3d, NUM_THRUSTERS> direction_vectors_;
    ThrustMap thrust_map_;
};
