#pragma once

#include "thrust_mapping.hpp"

// Transport-independent command scaling and PWM ramping.
class ThrustControl {
public:
    static constexpr int MAX_CHANGE = 5;
    static constexpr int NEUTRAL_PWM = 127;
    static constexpr int DEFAULT_POWER_MODE = 3;

    ThrustControl();
    ThrustMapper::Pwm Update(const ThrustMapper::Effort& effort, int power_mode);
    ThrustMapper::Pwm Neutral();

private:
    ThrustMapper mapper_;
    ThrustMapper::Pwm pwm_;
};
