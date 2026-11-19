#include "thrust_control.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#ifndef X19_THRUST_CONTROL_WITH_MESSAGING

ThrustControl::ThrustControl() {
    Neutral();
}

ThrustMapper::Pwm ThrustControl::Update(const ThrustMapper::Effort& effort, int power_mode) {
    constexpr std::array<double, 4> multipliers{1.0, 1.5, 3.0, 4.5};
    if (power_mode < 0 || power_mode >= static_cast<int>(multipliers.size())) {
        throw std::invalid_argument("power_mode must be between 0 and 3");
    }
    if (!effort.allFinite()) {
        throw std::invalid_argument("all six effort values must be finite");
    }
    const ThrustMapper::Effort base =
        (ThrustMapper::Effort() << 1.6, 1.6, 1.6, 10.0, 0.3, 0.6).finished();
    // Preserve the original controller's scaling; never mutate the input effort.
    const ThrustMapper::Effort scaled =
        effort.cwiseProduct(base) * (multipliers[power_mode] * 5.0);
    if (!scaled.allFinite()) {
        throw std::invalid_argument("scaled effort must be finite");
    }
    // Reject overflow before any floating-point to integer PWM conversion.
    if (!mapper_.GetThrust(scaled).allFinite()) {
        throw std::invalid_argument("mapped thrust must be finite");
    }
    const auto target = mapper_.GetPwm(scaled);
    for (std::size_t i = 0; i < pwm_.size(); ++i) {
        const int difference = static_cast<int>(target[i]) - static_cast<int>(pwm_[i]);
        pwm_[i] = static_cast<std::uint8_t>(
            static_cast<int>(pwm_[i]) + std::clamp(difference, -MAX_CHANGE, MAX_CHANGE));
    }
    return pwm_;
}

ThrustMapper::Pwm ThrustControl::Neutral() {
    pwm_.fill(NEUTRAL_PWM);
    return pwm_;
}

#else

#include "messaging/publisher.hpp"
#include "messaging/subscriber.hpp"
#include "telemetry.pb.h"
#include "thruster.pb.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void Stop(int) { stop_requested = 1; }

struct Options {
    std::string command_address = "tcp://127.0.0.1:5558";
    std::string thrust_address = "tcp://127.0.0.1:5559";
    std::string tools_address = "tcp://127.0.0.1:5560";
    int power_mode = ThrustControl::DEFAULT_POWER_MODE;
};

int ParseInteger(const std::string& value) {
    std::size_t consumed = 0;
    const int result = std::stoi(value, &consumed);
    if (consumed != value.size()) {
        throw std::invalid_argument("invalid integer: " + value);
    }
    return result;
}

Options ParseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string name = argv[i];
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value for " + name);
        }
        const std::string value = argv[++i];
        if (name == "--command-address") options.command_address = value;
        else if (name == "--thrust-address") options.thrust_address = value;
        else if (name == "--tools-address") options.tools_address = value;
        else if (name == "--power-mode") options.power_mode = ParseInteger(value);
        else throw std::invalid_argument("unknown option: " + name);
    }
    if (options.power_mode < 0 || options.power_mode > 3) {
        throw std::invalid_argument("--power-mode must be between 0 and 3");
    }
    return options;
}

class ThrustControlNode {
public:
    explicit ThrustControlNode(const Options& options)
        : options_(options),
          thrust_pub_(options.thrust_address, "final_thrust"),
          tools_pub_(options.tools_address, "tools_motor"),
          thrust_sub_(options.command_address, "thrust_command",
              [this](const rov::thruster::ThrustCommand& command) { PilotCommand(command); }),
          joystick_sub_(options.command_address, "joystick_command",
              [this](const rov::telemetry::JoystickCommand& command) { JoystickCommand(command); }),
          tools_sub_(options.command_address, "tools_command",
              [this](const rov::thruster::ToolsCommand& command) { ToolsCommand(command); }) {}

    void Run() {
        std::cout << "Commands: " << options_.command_address
                  << "; final_thrust: " << options_.thrust_address
                  << "; tools_motor: " << options_.tools_address << std::endl;
        while (!stop_requested) {
            try {
                // All sockets and callbacks stay on this thread.
                thrust_sub_.spinOnce(std::chrono::milliseconds{0});
                joystick_sub_.spinOnce(std::chrono::milliseconds{0});
                tools_sub_.spinOnce(std::chrono::milliseconds{0});
            } catch (const zmq::error_t& error) {
                if (error.num() != EINTR) throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        PublishThrust(controller_.Neutral());
    }

private:
    void PublishThrust(const ThrustMapper::Pwm& pwm) {
        rov::thruster::FinalThrust message;
        for (const auto value : pwm) message.add_thrusters(value);
        if (!thrust_pub_.publish(message)) {
            throw std::runtime_error("failed to publish final_thrust");
        }
    }

    void Apply(const ThrustMapper::Effort& effort, int power_mode) {
        PublishThrust(controller_.Update(effort, power_mode));
    }

    void PilotCommand(const rov::thruster::ThrustCommand& command) {
        if (command.desired_thrust_size() != 6 || command.power_mode() > 3) {
            std::cerr << "Rejected thrust_command: need six values and power_mode 0..3\n";
            return;
        }
        ThrustMapper::Effort effort;
        for (int i = 0; i < 6; ++i) effort(i) = command.desired_thrust(i);
        Apply(effort, static_cast<int>(command.power_mode()));
    }

    void JoystickCommand(const rov::telemetry::JoystickCommand& command) {
        ThrustMapper::Effort effort;
        // Joystick schema lists pitch before roll; mapper expects roll before pitch.
        effort << command.forward(), command.strafe(), command.vertical(),
                  command.roll(), command.pitch(), command.yaw();
        if (!effort.allFinite() || effort.cwiseAbs().maxCoeff() > 1.0) {
            std::cerr << "Rejected joystick_command: axes must be finite and within [-1, 1]\n";
            return;
        }
        Apply(effort, options_.power_mode);
    }

    void ToolsCommand(const rov::thruster::ToolsCommand& command) {
        rov::thruster::ToolsMotor message;
        for (const int value : command.tools()) {
            message.add_tools(static_cast<unsigned>(std::clamp(value, 0, 255)));
        }
        if (!tools_pub_.publish(message)) {
            throw std::runtime_error("failed to publish tools_motor");
        }
    }

    Options options_;
    ThrustControl controller_;
    CppMsg::Publisher thrust_pub_;
    CppMsg::Publisher tools_pub_;
    CppMsg::Subscriber<rov::thruster::ThrustCommand> thrust_sub_;
    CppMsg::Subscriber<rov::telemetry::JoystickCommand> joystick_sub_;
    CppMsg::Subscriber<rov::thruster::ToolsCommand> tools_sub_;
};
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: thrust_control [--command-address ENDPOINT] "
                     "[--thrust-address ENDPOINT] [--tools-address ENDPOINT] "
                     "[--power-mode 0..3]\n";
        return 0;
    }
    try {
        const auto options = ParseOptions(argc, argv);
        std::signal(SIGINT, Stop);
        std::signal(SIGTERM, Stop);
        ThrustControlNode node(options);
        node.Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "thrust_control: " << error.what() << '\n';
        return 1;
    }
}

#endif
