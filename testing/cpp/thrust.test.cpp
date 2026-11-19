#include <iostream>
#include "thrust_mapping.hpp"

int main() {
    ThrustMapper mapper;
    ThrustMapper::Effort effort;

    std::cout << "Enter 6 movement values (X Y Z Roll Pitch Yaw): ";

    for (int i = 0; i < 6; i++) {
        if (!(std::cin >> effort(i))) {
            std::cerr << "Invalid input\n";
            return 1;
        }
    }

    auto pwm = mapper.GetPwm(effort);

    std::cout << "PWM Output: ";

    for (int i = 0; i < 6; i++) {
        std::cout << static_cast<int>(pwm[i]) << " ";
    }

    std::cout << '\n';
    return 0;
}