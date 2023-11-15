// Copyright 2023 Autoware Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EKF_LOCALIZER__YAW_BIAS_MONITOR_HPP_
#define EKF_LOCALIZER__YAW_BIAS_MONITOR_HPP_

#include "ekf_localizer/simple_filter_base.hpp"

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist_with_covariance_stamped.hpp>

#include <string>

class YawBiasEstimator : public Simple1DFilter
{
public:
  YawBiasEstimator()
  : Simple1DFilter(),
    speed_lower_limit(2),
    rotation_speed_upper_limit(0.01),
    distance_upper_limit(10),
    distance_lower_limit(0.1){};

  void update(
    const geometry_msgs::msg::PoseWithCovarianceStamped & pose,
    const geometry_msgs::msg::TwistWithCovarianceStamped & twist, double obs_variance = 0.1)
  {
    double dx = pose.pose.pose.position.x - previous_ndt_pose_.pose.pose.position.x;
    double dy = pose.pose.pose.position.y - previous_ndt_pose_.pose.pose.position.y;
    double distance = std::sqrt(dx * dx + dy * dy);
    double estimated_yaw = std::atan2(dy, dx);
    double measured_yaw =
      std::atan2(pose.pose.pose.orientation.z, pose.pose.pose.orientation.w) * 2;

    double yaw_bias = measured_yaw - estimated_yaw;

    while (yaw_bias > M_PI / 2) {
      yaw_bias -= M_PI;
    }
    while (yaw_bias < -M_PI / 2) {
      yaw_bias += M_PI;
    }  // normalize to -pi/2 ~ pi/2

    double speed = std::abs(twist.twist.twist.linear.x);
    double rotation_speed = std::abs(twist.twist.twist.angular.z);
    previous_ndt_pose_ = pose;
    if (
      (speed < speed_lower_limit) || (rotation_speed > rotation_speed_upper_limit) ||
      (distance > distance_upper_limit) || (distance < distance_lower_limit)) {
      return;  // ignore when speed is low or rotation speed is high
    }
    Simple1DFilter::update(yaw_bias, obs_variance, pose.header.stamp);
  }

private:
  geometry_msgs::msg::PoseWithCovarianceStamped previous_ndt_pose_;
  double speed_lower_limit;
  double rotation_speed_upper_limit;
  double distance_upper_limit;
  double distance_lower_limit;
};

#endif  // EKF_LOCALIZER__YAW_BIAS_MONITOR_HPP_
