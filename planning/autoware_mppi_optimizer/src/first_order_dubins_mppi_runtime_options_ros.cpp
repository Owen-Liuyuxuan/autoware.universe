// Copyright 2026 TIER IV, Inc.
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

#include "autoware/mppi_optimizer/first_order_dubins_mppi_runtime_options_ros.hpp"

#include <string>

namespace autoware::mppi_optimizer
{
namespace
{

std::string param_name(const std::string & prefix, const std::string & name)
{
  return prefix.empty() ? name : prefix + name;
}

}  // namespace

void declare_first_order_dubins_mppi_runtime_options(
  rclcpp::Node & node, const std::string & prefix)
{
  const FirstOrderDubinsMppiRuntimeOptions defaults;
  node.declare_parameter(
    param_name(prefix, "enable_debug_trajectory_log"), defaults.enable_debug_trajectory_log);
  node.declare_parameter(
    param_name(prefix, "debug_trajectory_log_directory"), defaults.debug_trajectory_log_directory);
  node.declare_parameter(param_name(prefix, "ignore_obstacles"), defaults.ignore_obstacles);
  node.declare_parameter(param_name(prefix, "ignore_road_borders"), defaults.ignore_road_borders);
  node.declare_parameter(param_name(prefix, "ignore_drivable_area"), defaults.ignore_drivable_area);
  node.declare_parameter(
    param_name(prefix, "force_cold_start_each_step"), defaults.force_cold_start_each_step);
  node.declare_parameter(param_name(prefix, "skip_if_invalid"), defaults.skip_if_invalid);
  node.declare_parameter(
    param_name(prefix, "min_optimization_length"), defaults.min_optimization_length);
  node.declare_parameter(
    param_name(prefix, "use_last_control_as_nominal"), defaults.use_last_control_as_nominal);
  node.declare_parameter(
    param_name(prefix, "enable_dynamic_reseeding"), defaults.enable_dynamic_reseeding);
  node.declare_parameter(
    param_name(prefix, "dynamic_reseed_obstacle_cost_threshold"),
    defaults.dynamic_reseed_obstacle_cost_threshold);
  node.declare_parameter(
    param_name(prefix, "dynamic_reseed_road_border_cost_threshold"),
    defaults.dynamic_reseed_road_border_cost_threshold);
  node.declare_parameter(
    param_name(prefix, "evasive_rollout_fraction"), defaults.evasive_rollout_fraction);
  node.declare_parameter(
    param_name(prefix, "use_temporal_mpt_as_nominal"), defaults.use_temporal_mpt_as_nominal);
  node.declare_parameter(
    param_name(prefix, "prevent_reverse_velocity"), defaults.prevent_reverse_velocity);
  node.declare_parameter(
    param_name(prefix, "enable_input_delay_compensation"),
    defaults.enable_input_delay_compensation);
  node.declare_parameter(param_name(prefix, "enable_tube_feedback"), defaults.enable_tube_feedback);
  node.declare_parameter(param_name(prefix, "tube_velocity_gain"), defaults.tube_velocity_gain);
  node.declare_parameter(
    param_name(prefix, "tube_acceleration_gain"), defaults.tube_acceleration_gain);
  node.declare_parameter(
    param_name(prefix, "tube_lateral_position_gain"), defaults.tube_lateral_position_gain);
  node.declare_parameter(param_name(prefix, "tube_yaw_gain"), defaults.tube_yaw_gain);
  node.declare_parameter(param_name(prefix, "tube_steering_gain"), defaults.tube_steering_gain);
  node.declare_parameter(
    param_name(prefix, "steer_delay_residual_gain"), defaults.steer_delay_residual_gain);
}

FirstOrderDubinsMppiRuntimeOptions get_first_order_dubins_mppi_runtime_options(
  const rclcpp::Node & node, const std::string & prefix)
{
  FirstOrderDubinsMppiRuntimeOptions options;
  options.enable_debug_trajectory_log =
    node.get_parameter(param_name(prefix, "enable_debug_trajectory_log")).as_bool();
  options.debug_trajectory_log_directory =
    node.get_parameter(param_name(prefix, "debug_trajectory_log_directory")).as_string();
  options.ignore_obstacles = node.get_parameter(param_name(prefix, "ignore_obstacles")).as_bool();
  options.ignore_road_borders =
    node.get_parameter(param_name(prefix, "ignore_road_borders")).as_bool();
  options.ignore_drivable_area =
    node.get_parameter(param_name(prefix, "ignore_drivable_area")).as_bool();
  options.force_cold_start_each_step =
    node.get_parameter(param_name(prefix, "force_cold_start_each_step")).as_bool();
  options.skip_if_invalid = node.get_parameter(param_name(prefix, "skip_if_invalid")).as_bool();
  options.min_optimization_length = static_cast<float>(
    node.get_parameter(param_name(prefix, "min_optimization_length")).as_double());
  options.use_last_control_as_nominal =
    node.get_parameter(param_name(prefix, "use_last_control_as_nominal")).as_bool();
  options.enable_dynamic_reseeding =
    node.get_parameter(param_name(prefix, "enable_dynamic_reseeding")).as_bool();
  options.dynamic_reseed_obstacle_cost_threshold = static_cast<float>(
    node.get_parameter(param_name(prefix, "dynamic_reseed_obstacle_cost_threshold")).as_double());
  options.dynamic_reseed_road_border_cost_threshold = static_cast<float>(
    node.get_parameter(param_name(prefix, "dynamic_reseed_road_border_cost_threshold"))
      .as_double());
  options.evasive_rollout_fraction = static_cast<float>(
    node.get_parameter(param_name(prefix, "evasive_rollout_fraction")).as_double());
  options.use_temporal_mpt_as_nominal =
    node.get_parameter(param_name(prefix, "use_temporal_mpt_as_nominal")).as_bool();
  options.prevent_reverse_velocity =
    node.get_parameter(param_name(prefix, "prevent_reverse_velocity")).as_bool();
  options.enable_input_delay_compensation =
    node.get_parameter(param_name(prefix, "enable_input_delay_compensation")).as_bool();
  options.enable_tube_feedback =
    node.get_parameter(param_name(prefix, "enable_tube_feedback")).as_bool();
  options.tube_velocity_gain =
    static_cast<float>(node.get_parameter(param_name(prefix, "tube_velocity_gain")).as_double());
  options.tube_acceleration_gain = static_cast<float>(
    node.get_parameter(param_name(prefix, "tube_acceleration_gain")).as_double());
  options.tube_lateral_position_gain = static_cast<float>(
    node.get_parameter(param_name(prefix, "tube_lateral_position_gain")).as_double());
  options.tube_yaw_gain =
    static_cast<float>(node.get_parameter(param_name(prefix, "tube_yaw_gain")).as_double());
  options.tube_steering_gain =
    static_cast<float>(node.get_parameter(param_name(prefix, "tube_steering_gain")).as_double());
  options.steer_delay_residual_gain = static_cast<float>(
    node.get_parameter(param_name(prefix, "steer_delay_residual_gain")).as_double());
  return options;
}

}  // namespace autoware::mppi_optimizer
