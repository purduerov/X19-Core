#include "thrust_mapping.hpp"

#include <algorithm>

ThrustMapper::ThrustMapper()
    : invert_thrust_{1, 1, -1, -1, -1, 1},

    position_vectors_{{{-0.1981, 0.1852, 0.0699 + Z_OFFSET},
                        {0.1981, 0.1852, 0.0699 + Z_OFFSET},
                        {-0.1981, -0.1852, 0.0699 + Z_OFFSET},
                        {0.1981, -0.1852, 0.0699 + Z_OFFSET},
                        {0.2223, 0.0, 0.0},
                        {-0.2223, 0.0, 0.0}}},

    direction_vectors_{{{1.0, 1.0, 0.0},
                        {-1.0, 1.0, 0.0},
                        {1.0, -1.0, 0.0},
                        {-1.0, -1.0, 0.0},
                        {0.0, 0.0, 1.0},
                        {0.0, 0.0, 1.0}}} {
    thrust_map_ = Setup();
}

ThrustMapper::ThrustMap ThrustMapper::Setup() {
    for (int i = 0; i < NUM_THRUSTERS; ++i) {
        // Normalize the direction vector
        direction_vectors_[i].normalize();
    }

    Eigen::Matrix<double, 6, NUM_THRUSTERS> B;
    for (int i = 0; i < NUM_THRUSTERS; ++i) {
        B.block<3, 1>(0, i) = direction_vectors_[i];
        B.block<3, 1>(3, i) = position_vectors_[i].cross(direction_vectors_[i]);
    }

    // if the rank is less than 5, there is an issue so we should just return zeros
    // review later when it is 8 thrusters, but for now we will just check for 5
    Eigen::FullPivLU<Eigen::Matrix<double, 6, NUM_THRUSTERS>> lu(B);
    if (lu.rank() < 5) {
        return ThrustMap::Zero();
    }

    return B.completeOrthogonalDecomposition().pseudoInverse();
}

ThrustMapper::Thrust ThrustMapper::GetThrust(const Effort& effort) const {
    // find thrust_map * effort
    Thrust thrust = thrust_map_ * effort;

    // invert thrusters as desired
    for (int i = 0; i < NUM_THRUSTERS; ++i) {
        thrust(i) *= invert_thrust_[i];
    }
    return thrust;
}

// return a pwm value between 0 and 255 for each thruster. 127 is no thrust
ThrustMapper::Pwm ThrustMapper::GetPwm(const Effort& effort) const {
    Thrust thrust = GetThrust(effort);

    // clip the thrust to the allowable range
    thrust = thrust.cwiseMax(-THRUST_MAX).cwiseMin(THRUST_MAX);

    // normalize the thrust to 0 to 255
    return ThrustToPwm(thrust);
}

// maps a thrust between THRUST_MIN and THRUST_MAX to a pwm between 25 and 230
ThrustMapper::Pwm ThrustMapper::ThrustToPwm(const Thrust& thrust) {
    Pwm pwm{};
    for (int i = 0; i < NUM_THRUSTERS; ++i) {
        double value = std::clamp(thrust(i) / (THRUST_MAX - THRUST_MIN) + 0.5, 0.0, 1.0);
        value = value * 205.0 + 25.0;

        // numpy's int16 cast truncates toward zero, so a plain static_cast matches it
        auto counts = static_cast<std::int16_t>(value);
        counts = std::clamp<std::int16_t>(counts, 25, 230);

        pwm[i] = static_cast<std::uint8_t>(counts);
    }
    return pwm;
}
