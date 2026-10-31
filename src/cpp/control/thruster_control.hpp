/*
 * takes joystick command + depth control command and
 * outputs thruster command for ROV
 * space to be sent to thruster mappings
 */

#include <Eigen/Dense>
#include <Eigen/Geometry>

Eigen::Isometry3d genThrusterCommand(
    Eigen::AngleAxisd measuredPose,
    Eigen::Isometry3d joystickCommand,
    float depthControlCommand)
{
    // Rotation from local coordinates to world coordinates
    Eigen::Matrix3d R = measuredPose.toRotationMatrix();

    // Convert joystick translation to world coordinates
    Eigen::Vector3d worldCommand =
        R * joystickCommand.translation();

    // Set the depth controller command as the vertical component
    worldCommand.z() = depthControlCommand;

    // Convert back to local coordinates
    Eigen::Vector3d localCommand =
        R.transpose() * worldCommand;

    // Return the resulting isometry
    Eigen::Isometry3d result = joystickCommand;
    result.translation() = localCommand;

    return result;
}
