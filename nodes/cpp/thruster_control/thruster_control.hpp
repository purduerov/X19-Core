/*
 * takes joystick command + depth control command and
 * outputs thruster command for ROV
 * space to be sent to thruster mappings
 */

typedef struct {
  rov::telemetry::Vector3D position;
  rov::telemetry::Vector3D orientation; 
} Pose;

Pose genThrusterCommand(Pose measuredPose, Pose joystickCommand,
                                 float depthControlCommand) {

}
