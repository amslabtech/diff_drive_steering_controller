/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, PAL Robotics, S.L.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the PAL Robotics nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <control_msgs/JointTrajectoryControllerState.h>
#include <controller_interface/controller.h>
#include <controller_interface/multi_interface_controller.h>
#include <diff_drive_steering_controller/DiffDriveSteeringControllerConfig.h>
#include <diff_drive_steering_controller/odometry.h>
#include <diff_drive_steering_controller/speed_limiter.h>
#include <dynamic_reconfigure/server.h>
#include <geometry_msgs/TwistStamped.h>
#include <hardware_interface/joint_command_interface.h>
#include <memory>
#include <nav_msgs/Odometry.h>
#include <std_msgs/Float64.h>
#include <string>
#include <pluginlib/class_list_macros.hpp>
#include <realtime_tools/realtime_buffer.h>
#include <realtime_tools/realtime_publisher.h>
#include <tf/tfMessage.h>
#include <unordered_map>

namespace diff_drive_steering_controller{

  /**
   * This class makes some assumptions on the model of the robot:
   *  - the rotation axes of wheels are collinear
   *  - the wheels are identical in radius
   * Additional assumptions to not duplicate information readily available in the URDF:
   *  - the wheels have the same parent frame
   *  - a wheel collision geometry is a cylinder or sphere in the urdf
   *  - a wheel joint frame center's vertical projection on the floor must lie within the contact patch
   */
  class DiffDriveSteeringController
      : public controller_interface::MultiInterfaceController<hardware_interface::VelocityJointInterface, hardware_interface::PositionJointInterface>
  {
  public:
    DiffDriveSteeringController();

    /**
     * \brief Initialize controller
     * \param hw            Velocity joint interface for the wheels
     * \param root_nh       Node handle at root namespace
     * \param controller_nh Node handle inside the controller namespace
     */
    bool init(hardware_interface::RobotHW* hw,
              ros::NodeHandle& root_nh,
              ros::NodeHandle &controller_nh);

    /**
     * \brief Updates controller, i.e. computes the odometry and sets the new velocity commands
     * \param time   Current time
     * \param period Time since the last called to update
     */
    void update(const ros::Time& time, const ros::Duration& period);

    /**
     * \brief Starts controller
     * \param time Current time
     */
    void starting(const ros::Time& time);

    /**
     * \brief Stops controller
     * \param time Current time
     */
    void stopping(const ros::Time& /*time*/);

  private:
    std::string name_;

    /// Odometry related:
    ros::Duration publish_period_;
    ros::Time last_state_publish_time_;
    bool open_loop_;

    /// Hardware handles:
    hardware_interface::JointHandle left_wheel_joint_;
    hardware_interface::JointHandle right_wheel_joint_;
    hardware_interface::JointHandle left_steering_joint_;
    hardware_interface::JointHandle right_steering_joint_;

    // Previous time
    ros::Time time_previous_;

    /// Previous velocities from the encoders:
    std::vector<double> vel_left_previous_;
    std::vector<double> vel_right_previous_;

    /// Previous velocities from the encoders:
    double vel_left_desired_previous_;
    double vel_right_desired_previous_;

    /// Velocity command related:
    struct Commands
    {
      double vx;
      double vy;
      double wz;
      ros::Time stamp;

      Commands() : vx(0.0), vy(0.0), wz(0.0), stamp(0.0) {}
    };
    realtime_tools::RealtimeBuffer<Commands> command_;
    Commands command_struct_;
    ros::Subscriber sub_command_;
    std::unordered_map<std::string, ros::Subscriber> sub_steering_command_;
    std::unordered_map<std::string, double> steering_target_angle_;
    bool enable_independent_steering_;

    /// Odometry related:
    std::shared_ptr<realtime_tools::RealtimePublisher<nav_msgs::Odometry> > odom_pub_;
    std::shared_ptr<realtime_tools::RealtimePublisher<tf::tfMessage> > tf_odom_pub_;
    Odometry odometry_;

    /// Wheel separation, wrt the midpoint of the wheel width:
    double wheel_separation_;

    /// Wheel radius (assuming it's the same for the left and right wheels):
    double wheel_radius_;

    /// Wheel separation and radius calibration multipliers:
    double wheel_separation_multiplier_;
    double left_wheel_radius_multiplier_;
    double right_wheel_radius_multiplier_;

    /// Timeout to consider cmd_vel commands old:
    double cmd_vel_timeout_;

    /// Whether to allow multiple publishers on cmd_vel topic or not:
    bool allow_multiple_cmd_vel_publishers_;

    /// Frame to use for the robot base:
    std::string base_frame_id_;

    /// Frame to use for odometry and odom tf:
    std::string odom_frame_id_;

    /// Whether to publish odometry to tf or not:
    bool enable_odom_tf_;

    /// Speed limiters:
    Commands last1_cmd_;
    Commands last0_cmd_;
    SpeedLimiter limiter_lin_;
    SpeedLimiter limiter_ang_;

	/// Steering angle limit 
	double steering_angle_absolute_value_limit_;

    // A struct to hold dynamic parameters
    // set from dynamic_reconfigure server
    struct DynamicParams
    {
      bool update;

      double left_wheel_radius_multiplier;
      double right_wheel_radius_multiplier;
      double wheel_separation_multiplier;

      bool publish_cmd;

      double publish_rate;
      bool enable_odom_tf;

      DynamicParams()
        : left_wheel_radius_multiplier(1.0)
        , right_wheel_radius_multiplier(1.0)
        , wheel_separation_multiplier(1.0)
        , publish_cmd(false)
        , publish_rate(50)
        , enable_odom_tf(true)
      {}

      friend std::ostream& operator<<(std::ostream& os, const DynamicParams& params)
      {
        os << "DynamicParams:\n"
           //
           << "\tOdometry parameters:\n"
           << "\t\tleft wheel radius multiplier: "   << params.left_wheel_radius_multiplier  << "\n"
           << "\t\tright wheel radius multiplier: "  << params.right_wheel_radius_multiplier << "\n"
           << "\t\twheel separation multiplier: "    << params.wheel_separation_multiplier   << "\n"
           //
           << "\tPublication parameters:\n"
           << "\t\tPublish executed velocity command: " << (params.publish_cmd?"enabled":"disabled") << "\n"
           << "\t\tPublication rate: " << params.publish_rate                 << "\n"
           << "\t\tPublish frame odom on tf: " << (params.enable_odom_tf?"enabled":"disabled");

        return os;
      }
    };

    realtime_tools::RealtimeBuffer<DynamicParams> dynamic_params_;

    /// Dynamic Reconfigure server
    typedef dynamic_reconfigure::Server<DiffDriveSteeringControllerConfig> ReconfigureServer;
    
    std::shared_ptr<ReconfigureServer> dyn_reconf_server_;

  private:
    /**
     * \brief Brakes the wheels, i.e. sets the velocity to 0
     */
    void brake();

    /**
     * \brief Velocity command callback
     * \param command Velocity command message (twist)
     */
    void cmdVelCallback(const geometry_msgs::Twist& command);

    /**
     * \brief Steering command callback
     *
     * \param command Steering angle in radians.
     * \param wheel_name "left" or "right"
     */
    void steeringCmdCallback(const std_msgs::Float64ConstPtr& command, const std::string& wheel_name);

    /**
     * \brief Get the wheel names from a wheel param
     * \param [in]  controller_nh Controller node handler
     * \param [in]  wheel_param   Param name
     * \param [out] wheel_names   Vector with the whel names
     * \return true if the wheel_param is available and the wheel_names are
     *        retrieved successfully from the param server; false otherwise
     */
    bool getWheelNames(ros::NodeHandle& controller_nh,
                       const std::string& wheel_param,
                       std::vector<std::string>& wheel_names);

    /**
     * \brief Sets odometry parameters from the URDF, i.e. the wheel radius and separation
     * \param root_nh Root node handle
     * \param left_wheel_name Name of the left wheel joint
     * \param right_wheel_name Name of the right wheel joint
     */
    bool setOdomParamsFromUrdf(ros::NodeHandle& root_nh,
                               const std::string& left_wheel_name,
                               const std::string& right_wheel_name,
                               bool lookup_wheel_separation,
                               bool lookup_wheel_radius);

    /**
     * \brief Sets the odometry publishing fields
     * \param root_nh Root node handle
     * \param controller_nh Node handle inside the controller namespace
     */
    void setOdomPubFields(ros::NodeHandle& root_nh, ros::NodeHandle& controller_nh);

    /**
     * \brief Callback for dynamic_reconfigure server
     * \param config The config set from dynamic_reconfigure server
     * \param level not used at this time.
     * \see dyn_reconf_server_
     */
    void reconfCallback(DiffDriveSteeringControllerConfig& config, uint32_t /*level*/);

    /**
     * \brief Update the dynamic parameters in the RT loop
     */
    void updateDynamicParams();

  };

  PLUGINLIB_EXPORT_CLASS(diff_drive_steering_controller::DiffDriveSteeringController, controller_interface::ControllerBase);
} // namespace diff_drive_steering_controller
