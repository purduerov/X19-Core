/*
 * takes joystick command + depth control command and
 * outputs thruster command for ROV
 * space to be sent to thruster mappings
 */
#pragma once

#include <optional>

#include <Eigen/Dense>
#include <Eigen/Geometry>

// depthControlCommand: world-frame vertical command from the depth controller,
// or std::nullopt when depth hold is inactive (e.g. the pilot is holding the
// override button) -- in that case the pilot's own vertical input is used.
inline Eigen::Isometry3d genThrusterCommand(
    Eigen::AngleAxisd measuredPose,
    Eigen::Isometry3d joystickCommand,
    std::optional<float> depthControlCommand)
{
    // Rotation from local coordinates to world coordinates
    Eigen::Matrix3d R = measuredPose.toRotationMatrix();

    // Convert joystick translation to world coordinates
    Eigen::Vector3d worldCommand =
        R * joystickCommand.translation();

    // If depth hold is active, it owns the vertical axis
    if (depthControlCommand.has_value()) {
        worldCommand.z() = *depthControlCommand;
    }

    // Convert back to local coordinates
    Eigen::Vector3d localCommand =
        R.transpose() * worldCommand;

    // Return the resulting isometry
    Eigen::Isometry3d result = joystickCommand;
    result.translation() = localCommand;

    return result;
}
