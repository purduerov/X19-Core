#include "thrust_control.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void Rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "invalid command was accepted");
}
}

int main() {
    try {
        ThrustMapper mapper;
        const auto zero = ThrustMapper::Effort::Zero().eval();
        for (const auto value : mapper.GetPwm(zero)) Check(value == 127, "zero must be neutral");
        Check(mapper.thrust_map_.allFinite(), "mapping must be finite");
        const ThrustMapper::Effort base =
            (ThrustMapper::Effort() << 1.6, 1.6, 1.6, 10.0, 0.3, 0.6).finished();
        constexpr double multipliers[] = {1.0, 1.5, 3.0, 4.5};
        for (int mode = 0; mode < 4; ++mode) {
            for (int axis = 0; axis < 6; ++axis) {
                for (const double sign : {-1.0, 1.0}) {
                    ThrustControl control;
                    auto effort = zero;
                    effort(axis) = sign * 0.125;
                    const auto original = effort;
                    const auto expected = mapper.GetPwm(
                        effort.cwiseProduct(base) * (multipliers[mode] * 5.0));
                    auto previous = control.Neutral();
                    for (int step = 0; step < 60; ++step) {
                        const auto actual = control.Update(effort, mode);
                        for (std::size_t i = 0; i < actual.size(); ++i) {
                            const int delta = static_cast<int>(expected[i]) - static_cast<int>(previous[i]);
                            Check(actual[i] == static_cast<int>(previous[i]) + std::clamp(delta, -5, 5),
                                  "ramp or scaling changed");
                            Check(actual[i] >= 25 && actual[i] <= 230, "PWM bounds");
                        }
                        previous = actual;
                    }
                    Check(previous == expected, "ramp did not converge");
                    Check(effort == original, "command was mutated");
                    for (const auto value : control.Neutral()) Check(value == 127, "neutral reset");
                }
            }
        }
        ThrustControl control;
        Rejects([&] { control.Update(zero, -1); });
        Rejects([&] { control.Update(zero, 4); });
        for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::max()}) {
            auto effort = zero;
            effort(0) = invalid;
            Rejects([&] { control.Update(effort, 3); });
        }
        std::cout << "Mapping, all axes/modes, ramping, and validation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
