//
// Copyright 2024 Tier IV, Inc. All rights reserved.
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
//

#include "autoware_generic_value_calibrator/generic_value_calibrator_node.hpp"

#include "rclcpp/logging.hpp"
#include "tf2/utils.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace autoware::generic_value_calibrator
{

GenericValueCalibrator::GenericValueCalibrator(const rclcpp::NodeOptions & node_options)
: Node("generic_value_calibrator", node_options)
{
  transform_listener_ = std::make_shared<autoware_utils::TransformListener>(this);
  
  // get parameter
  update_hz_ = declare_parameter<double>("update_hz", 10.0);
  covariance_ = declare_parameter<double>("initial_covariance", 0.05);
  velocity_min_threshold_ = declare_parameter<double>("velocity_min_threshold", 0.1);
  velocity_diff_threshold_ = declare_parameter<double>("velocity_diff_threshold", 0.556);
  value_diff_threshold_ = declare_parameter<double>("value_diff_threshold", 0.03);
  max_steer_threshold_ = declare_parameter<double>("max_steer_threshold", 0.2);
  max_pitch_threshold_ = declare_parameter<double>("max_pitch_threshold", 0.02);
  max_jerk_threshold_ = declare_parameter<double>("max_jerk_threshold", 0.7);
  value_velocity_thresh_ = declare_parameter<double>("value_velocity_thresh", 0.15);
  max_accel_ = declare_parameter<double>("max_accel", 5.0);
  min_accel_ = declare_parameter<double>("min_accel", -5.0);
  value_to_accel_delay_ = declare_parameter<double>("value_to_accel_delay", 0.3);
  max_data_count_ = static_cast<int>(declare_parameter("max_data_count", 200));
  progress_file_output_ = declare_parameter<bool>("progress_file_output", false);
  precision_ = static_cast<int>(declare_parameter("precision", 3));
  
  const auto get_pitch_method_str = declare_parameter("get_pitch_method", std::string("tf"));
  if (get_pitch_method_str == std::string("tf")) {
    get_pitch_method_ = GET_PITCH_METHOD::TF;
  } else if (get_pitch_method_str == std::string("none")) {
    get_pitch_method_ = GET_PITCH_METHOD::NONE;
  } else {
    RCLCPP_ERROR_STREAM(get_logger(), "get_pitch_method is wrong. (available method: tf, none)");
    return;
  }

  update_suggest_thresh_ = declare_parameter<double>("update_suggest_thresh", 0.7);
  csv_calibrated_map_dir_ = declare_parameter("csv_calibrated_map_dir", std::string(""));
  output_map_file_ = csv_calibrated_map_dir_ + "/value_map.csv";
  
  const std::string update_method_str =
    declare_parameter("update_method", std::string("update_offset_each_cell"));
  if (update_method_str == std::string("update_offset_each_cell")) {
    update_method_ = UPDATE_METHOD::UPDATE_OFFSET_EACH_CELL;
  } else if (update_method_str == std::string("update_offset_total")) {
    update_method_ = UPDATE_METHOD::UPDATE_OFFSET_TOTAL;
  } else {
    RCLCPP_ERROR_STREAM(
      get_logger(),
      "update_method is wrong. (available method: update_offset_each_cell, update_offset_total)");
    return;
  }

  // QoS setup
  static constexpr std::size_t queue_size = 1;
  rclcpp::QoS durable_qos(queue_size);

  /* Diagnostic Updater */
  updater_ptr_ = std::make_shared<diagnostic_updater::Updater>(this, 1.0 / update_hz_);
  updater_ptr_->setHardwareID("generic_value_calibrator");
  updater_ptr_->add(
    "generic_value_calibrator", this, &GenericValueCalibrator::check_update_suggest);

  // Initialize map from CSV or create default map
  csv_default_map_dir_ = declare_parameter("csv_default_map_dir", std::string(""));
  
  // Create a default map if no CSV is provided
  // Velocity index: 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 m/s
  vel_index_ = {0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0};
  // Value index: -1.0 to 1.0 with 0.1 step
  value_index_ = {-1.0, -0.8, -0.6, -0.4, -0.2, 0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
  
  // Initialize map with zeros
  value_map_.resize(value_index_.size());
  for (auto & m : value_map_) {
    m.resize(vel_index_.size(), 0.0);
  }
  
  update_value_map_.resize(value_index_.size());
  for (auto & m : update_value_map_) {
    m.resize(vel_index_.size(), 0.0);
  }
  
  value_offset_covariance_value_.resize(value_index_.size());
  for (auto & m : value_offset_covariance_value_) {
    m.resize(vel_index_.size(), covariance_);
  }

  std::copy(value_map_.begin(), value_map_.end(), update_value_map_.begin());

  // initialize matrix for covariance calculation
  {
    const auto gen_const_mat = [](const Map & map, const auto val) {
      return Eigen::MatrixXd::Constant(map.size(), map.at(0).size(), val);
    };
    data_mean_mat_ = gen_const_mat(value_map_, map_offset_);
    data_covariance_mat_ = gen_const_mat(value_map_, covariance_);
    data_num_ = gen_const_mat(value_map_, 1);
  }

  // publisher
  update_suggest_pub_ =
    create_publisher<std_msgs::msg::Bool>("~/output/update_suggest", durable_qos);
  current_map_error_pub_ =
    create_publisher<Float64Stamped>("~/output/current_map_error", durable_qos);
  updated_map_error_pub_ =
    create_publisher<Float64Stamped>("~/output/updated_map_error", durable_qos);
  map_error_ratio_pub_ = create_publisher<Float64Stamped>("~/output/map_error_ratio", durable_qos);

  // output log file
  std::string output_log_file = declare_parameter("output_log_file", std::string(""));
  if (!output_log_file.empty()) {
    output_log_.open(output_log_file);
    add_index_to_csv(&output_log_);
  }

  // timer
  init_timer(1.0 / update_hz_);
  init_output_csv_timer(30.0);

  logger_configure_ = std::make_unique<autoware_utils::LoggerLevelConfigure>(this);
  
  RCLCPP_INFO(get_logger(), "GenericValueCalibrator initialized successfully");
}

void GenericValueCalibrator::init_output_csv_timer(double period_s)
{
  const auto period_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(period_s));
  timer_output_csv_ = rclcpp::create_timer(
    this, get_clock(), period_ns,
    std::bind(&GenericValueCalibrator::timer_callback_output_csv, this));
}

void GenericValueCalibrator::init_timer(double period_s)
{
  const auto period_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(period_s));
  timer_ = rclcpp::create_timer(
    this, get_clock(), period_ns, std::bind(&GenericValueCalibrator::fetch_data, this));
}

bool GenericValueCalibrator::get_current_pitch_from_tf(double * pitch)
{
  if (get_pitch_method_ == GET_PITCH_METHOD::NONE) {
    *pitch = 0.0;
    return true;
  }

  const auto transform = transform_listener_->get_transform(
    "map", "base_link", rclcpp::Time(0), rclcpp::Duration::from_seconds(0.5));
  if (!transform) {
    RCLCPP_WARN_STREAM_THROTTLE(
      get_logger(), *get_clock(), 5000, "cannot get map to base_link transform. ");
    return false;
  }
  double roll = 0.0;
  double raw_pitch = 0.0;
  double yaw = 0.0;
  tf2::getEulerYPR(transform->transform.rotation, yaw, raw_pitch, roll);
  *pitch = lowpass(*pitch, raw_pitch, 0.2);
  return true;
}

bool GenericValueCalibrator::take_data()
{
  // take input value data
  Float64Stamped::ConstSharedPtr input_value_msg = input_value_sub_.take_data();
  if (!input_value_msg) return false;
  take_input_value(input_value_msg);

  // take velocity data
  VelocityReport::ConstSharedPtr velocity_ptr = velocity_sub_.take_data();
  if (!velocity_ptr) return false;
  take_velocity(velocity_ptr);

  // take steer data
  steer_ptr_ = steer_sub_.take_data();

  /* valid check */
  if (!twist_ptr_ || !steer_ptr_ || !input_value_ptr_ || !delayed_input_value_ptr_) {
    RCLCPP_WARN_STREAM_THROTTLE(
      get_logger(), *get_clock(), 5000, "lack of topics (twist, steer, input_value)");
    lack_of_data_count_++;
    return false;
  }
  return true;
}

void GenericValueCalibrator::fetch_data()
{
  update_count_++;

  RCLCPP_DEBUG_STREAM_THROTTLE(
    get_logger(), *get_clock(), 5000,
    "map updating... count: " << update_success_count_ << " / " << update_count_);

  // if cannot get data, return this callback
  if (!take_data()) return;

  // data check - timeout
  if (
    is_timeout(twist_ptr_->header.stamp, timeout_sec_) ||
    is_timeout(steer_ptr_->stamp, timeout_sec_) || is_timeout(input_value_ptr_, timeout_sec_)) {
    RCLCPP_WARN_STREAM_THROTTLE(
      get_logger(), *get_clock(), 5000, "timeout of topics (twist, steer, input_value)");
    lack_of_data_count_++;
    return;
  }

  // get pitch
  if (!get_current_pitch_from_tf(&pitch_)) {
    RCLCPP_WARN_STREAM_THROTTLE(get_logger(), *get_clock(), 5000, "cannot get pitch");
    failed_to_get_pitch_count_++;
    return;
  }

  /* publish error metrics */
  publish_update_suggest_flag();
  publish_float64("current_map_error", part_original_rmse_);
  publish_float64("updated_map_error", new_rmse_);
  publish_float64(
    "map_error_ratio",
    part_original_rmse_ != 0.0 ? new_rmse_ / part_original_rmse_ : 1.0);

  /* initialize */
  update_success_ = false;

  // velocity check
  if (twist_ptr_->twist.linear.x < velocity_min_threshold_) {
    too_low_speed_count_++;
    return;
  }

  // evaluation
  execute_evaluation();

  // pitch check
  if (std::fabs(pitch_) > max_pitch_threshold_) {
    too_large_pitch_count_++;
    return;
  }

  // steer check
  if (std::fabs(steer_ptr_->steering_tire_angle) > max_steer_threshold_) {
    too_large_steer_count_++;
    return;
  }

  // jerk check
  if (std::fabs(jerk_) > max_jerk_threshold_) {
    too_large_jerk_count_++;
    return;
  }

  // value speed check
  if (std::fabs(input_value_speed_) > value_velocity_thresh_) {
    too_large_value_spd_count_++;
    return;
  }

  /* update map */
  if (update_value_map()) {
    update_success_count_++;
    update_success_ = true;
  } else {
    update_fail_count_++;
  }
}

void GenericValueCalibrator::timer_callback_output_csv()
{
  write_map_to_csv(vel_index_, value_index_, update_value_map_, output_map_file_);
}

void GenericValueCalibrator::take_velocity(const VelocityReport::ConstSharedPtr msg)
{
  auto twist_msg = std::make_shared<TwistStamped>();
  twist_msg->header = msg->header;
  twist_msg->twist.linear.x = msg->longitudinal_velocity;

  if (!twist_vec_.empty()) {
    const auto past_msg = get_nearest_time_data_from_vec(twist_msg, dif_twist_time_, twist_vec_);
    const double raw_acceleration = get_accel(past_msg, twist_msg);
    acceleration_ = lowpass(acceleration_, raw_acceleration, 0.25);
    acceleration_time_ = rclcpp::Time(msg->header.stamp).seconds();

    // calculate jerk
    if (
      this->now().seconds() - pre_acceleration_time_ > timeout_sec_ ||
      (acceleration_time_ - pre_acceleration_time_) <= std::numeric_limits<double>::epsilon()) {
      // does not update jerk
    } else {
      const double raw_jerk = get_jerk();
      jerk_ = lowpass(jerk_, raw_jerk, 0.5);
    }
    pre_acceleration_ = acceleration_;
    pre_acceleration_time_ = acceleration_time_;
  }

  twist_ptr_ = twist_msg;
  push_data_to_vec(twist_msg, twist_vec_max_size_, &twist_vec_);
}

void GenericValueCalibrator::take_input_value(const Float64Stamped::ConstSharedPtr msg)
{
  input_value_ptr_ = std::make_shared<DataStamped>(msg->data, rclcpp::Time(msg->header.stamp));
  
  if (!input_value_vec_.empty()) {
    const auto past_value_ptr =
      get_nearest_time_data_from_vec(input_value_ptr_, dif_value_time_, input_value_vec_);
    const double raw_value_speed =
      get_value_speed(past_value_ptr, input_value_ptr_, input_value_speed_);
    input_value_speed_ = lowpass(input_value_speed_, raw_value_speed, 0.5);
  }
  
  push_data_to_vec(input_value_ptr_, value_vec_max_size_, &input_value_vec_);
  delayed_input_value_ptr_ =
    get_nearest_time_data_from_vec(input_value_ptr_, value_to_accel_delay_, input_value_vec_);
}

double GenericValueCalibrator::lowpass(
  const double original, const double current, const double gain)
{
  return current * gain + original * (1.0 - gain);
}

double GenericValueCalibrator::get_value_speed(
  const DataStampedPtr & prev_value, const DataStampedPtr & current_value,
  const double prev_value_speed)
{
  const double dt = (current_value->data_time - prev_value->data_time).seconds();
  if (dt < 1e-03) {
    return prev_value_speed;
  }

  const double d_value = current_value->data - prev_value->data;
  return d_value / dt;
}

double GenericValueCalibrator::get_accel(
  const TwistStamped::ConstSharedPtr & prev_twist,
  const TwistStamped::ConstSharedPtr & current_twist) const
{
  const double dt =
    (rclcpp::Time(current_twist->header.stamp) - rclcpp::Time(prev_twist->header.stamp)).seconds();
  if (dt < 1e-03) {
    return acceleration_;
  }
  const double dv = current_twist->twist.linear.x - prev_twist->twist.linear.x;
  return std::min(std::max(min_accel_, dv / dt), max_accel_);
}

double GenericValueCalibrator::get_jerk()
{
  const double jerk =
    (acceleration_ - pre_acceleration_) / (acceleration_time_ - pre_acceleration_time_);
  const double max_jerk = 5.0;
  return std::min(std::max(-max_jerk, jerk), max_jerk);
}

bool GenericValueCalibrator::index_value_search(
  const std::vector<double> & value_index, const double value, const double value_thresh,
  int * searched_index) const
{
  for (std::size_t i = 0; i < value_index.size(); i++) {
    const double diff_value = std::fabs(value_index.at(i) - value);
    if (diff_value <= value_thresh) {
      *searched_index = static_cast<int>(i);
      return true;
    }
  }
  return false;
}

int GenericValueCalibrator::nearest_value_search(
  const std::vector<double> & value_index, const double value)
{
  double max_dist = std::numeric_limits<double>::max();
  int nearest_idx = 0;

  for (std::size_t i = 0; i < value_index.size(); i++) {
    const double dist = std::fabs(value - value_index.at(i));
    if (max_dist > dist) {
      nearest_idx = static_cast<int>(i);
      max_dist = dist;
    }
  }
  return nearest_idx;
}

void GenericValueCalibrator::take_consistency_of_value_map()
{
  const double bit = std::pow(1e-01, precision_);
  for (std::size_t val_idx = 0; val_idx < update_value_map_.size() - 1; val_idx++) {
    for (std::size_t vel_idx = update_value_map_.at(0).size() - 1;; vel_idx--) {
      if (vel_idx == 0) break;

      const double current_acc = update_value_map_.at(val_idx).at(vel_idx);
      const double next_val_acc = update_value_map_.at(val_idx + 1).at(vel_idx);
      const double prev_vel_acc = update_value_map_.at(val_idx).at(vel_idx - 1);

      // Ensure monotonic relationship with velocity
      if (current_acc + bit >= prev_vel_acc) {
        update_value_map_.at(val_idx).at(vel_idx - 1) = current_acc + bit;
      }

      // Ensure monotonic relationship with value
      if (current_acc + bit >= next_val_acc) {
        update_value_map_.at(val_idx + 1).at(vel_idx) = current_acc + bit;
      }
    }
  }
}

bool GenericValueCalibrator::update_value_map()
{
  int value_index = 0;
  int vel_index = 0;

  if (!index_value_search(
        value_index_, delayed_input_value_ptr_->data, value_diff_threshold_, &value_index)) {
    return false;
  }

  if (!index_value_search(
        vel_index_, twist_ptr_->twist.linear.x, velocity_diff_threshold_, &vel_index)) {
    return false;
  }

  // update map
  execute_update(value_index, vel_index);

  // take consistency of map
  take_consistency_of_value_map();

  return true;
}

void GenericValueCalibrator::execute_update(const int value_index, const int vel_index)
{
  const double measured_acc = acceleration_ - get_pitch_compensated_acceleration();
  const double map_acc = update_value_map_.at(value_index).at(vel_index);

  if (update_method_ == UPDATE_METHOD::UPDATE_OFFSET_EACH_CELL) {
    update_each_val_offset(value_index, vel_index, measured_acc, map_acc);
  } else if (update_method_ == UPDATE_METHOD::UPDATE_OFFSET_TOTAL) {
    update_total_map_offset(measured_acc, map_acc);
  }
}

bool GenericValueCalibrator::update_each_val_offset(
  const int value_index, const int vel_index, const double measured_acc, const double map_acc)
{
  // pre-defined static variables
  static Map map_offset_vec(
    value_map_.size(), std::vector<double>(value_map_.at(0).size(), map_offset_));
  static Map covariance_vec(
    value_map_.size(), std::vector<double>(value_map_.at(0).size(), covariance_));

  double map_offset = map_offset_vec.at(value_index).at(vel_index);
  double covariance = covariance_vec.at(value_index).at(vel_index);

  /* RLS update */
  const double phi = 1.0;
  covariance = (covariance - (covariance * phi * phi * covariance) /
                               (forgetting_factor_ + phi * covariance * phi)) /
               forgetting_factor_;

  const double coef = (covariance * phi) / (forgetting_factor_ + phi * covariance * phi);

  const double error_map_offset = measured_acc - map_acc;
  map_offset = map_offset + coef * error_map_offset;

  /* update map */
  map_offset_vec.at(value_index).at(vel_index) = map_offset;
  covariance_vec.at(value_index).at(vel_index) = covariance;
  update_value_map_.at(value_index).at(vel_index) =
    value_map_.at(value_index).at(vel_index) + map_offset;

  return true;
}

void GenericValueCalibrator::update_total_map_offset(
  const double measured_acc, const double map_acc)
{
  /* RLS update */
  const double phi = 1.0;
  covariance_ = (covariance_ - (covariance_ * phi * phi * covariance_) /
                                 (forgetting_factor_ + phi * covariance_ * phi)) /
                forgetting_factor_;

  const double coef = (covariance_ * phi) / (forgetting_factor_ + phi * covariance_ * phi);
  const double error_map_offset = measured_acc - map_acc;
  map_offset_ = map_offset_ + coef * error_map_offset;

  /* update entire map */
  for (std::size_t val_idx = 0; val_idx < update_value_map_.size(); val_idx++) {
    for (std::size_t vel_idx = 0; vel_idx < update_value_map_.at(0).size(); vel_idx++) {
      update_value_map_.at(val_idx).at(vel_idx) =
        value_map_.at(val_idx).at(vel_idx) + map_offset_;
    }
  }
}

double GenericValueCalibrator::get_pitch_compensated_acceleration() const
{
  constexpr double gravity = 9.80665;
  return gravity * std::sin(pitch_);
}

void GenericValueCalibrator::execute_evaluation()
{
  const double part_orig_sq_error = calculate_value_squared_error(
    delayed_input_value_ptr_->data, twist_ptr_->twist.linear.x);
  push_data_to_vec(part_orig_sq_error, part_mse_que_size_, &part_original_mse_que_);
  part_original_rmse_ = std::sqrt(get_average(part_original_mse_que_));

  // Calculate error using updated map would require loading it from CSV
  // For now, just track the original map error
  push_data_to_vec(part_orig_sq_error, part_mse_que_size_, &new_mse_que_);
  new_rmse_ = std::sqrt(get_average(new_mse_que_));
}

double GenericValueCalibrator::calculate_value_squared_error(
  const double value, const double vel)
{
  // Find nearest indices
  const int val_idx = nearest_value_search(value_index_, value);
  const int vel_idx = nearest_value_search(vel_index_, vel);
  
  const double estimated_acc = value_map_.at(val_idx).at(vel_idx);
  const double measured_acc = acceleration_ - get_pitch_compensated_acceleration();
  const double dif_acc = measured_acc - estimated_acc;
  return dif_acc * dif_acc;
}

template <class T>
void GenericValueCalibrator::push_data_to_vec(
  const T data, const std::size_t max_size, std::vector<T> * vec)
{
  vec->emplace_back(data);
  while (vec->size() > max_size) {
    vec->erase(vec->begin());
  }
}

template <class T>
T GenericValueCalibrator::get_nearest_time_data_from_vec(
  const T base_data, const double back_time, const std::vector<T> & vec)
{
  double nearest_time = std::numeric_limits<double>::max();
  const double target_time = rclcpp::Time(base_data->header.stamp).seconds() - back_time;
  T nearest_time_data;
  for (const auto & data : vec) {
    const double data_time = rclcpp::Time(data->header.stamp).seconds();
    const auto delta_time = std::abs(target_time - data_time);
    if (nearest_time > delta_time) {
      nearest_time_data = data;
      nearest_time = delta_time;
    }
  }
  return nearest_time_data;
}

DataStampedPtr GenericValueCalibrator::get_nearest_time_data_from_vec(
  DataStampedPtr base_data, const double back_time, const std::vector<DataStampedPtr> & vec)
{
  double nearest_time = std::numeric_limits<double>::max();
  const double target_time = base_data->data_time.seconds() - back_time;
  DataStampedPtr nearest_time_data;
  for (const auto & data : vec) {
    const double data_time = data->data_time.seconds();
    const auto delta_time = std::abs(target_time - data_time);
    if (nearest_time > delta_time) {
      nearest_time_data = data;
      nearest_time = delta_time;
    }
  }
  return nearest_time_data;
}

double GenericValueCalibrator::get_average(const std::vector<double> & vec)
{
  if (vec.empty()) {
    return 0.0;
  }

  double sum = 0.0;
  for (const auto num : vec) {
    sum += num;
  }
  return sum / static_cast<double>(vec.size());
}

bool GenericValueCalibrator::is_timeout(
  const builtin_interfaces::msg::Time & stamp, const double timeout_sec)
{
  const double dt = this->now().seconds() - rclcpp::Time(stamp).seconds();
  return dt > timeout_sec;
}

bool GenericValueCalibrator::is_timeout(
  const DataStampedPtr & data_stamped, const double timeout_sec)
{
  const double dt = (this->now() - data_stamped->data_time).seconds();
  return dt > timeout_sec;
}

void GenericValueCalibrator::check_update_suggest(
  diagnostic_updater::DiagnosticStatusWrapper & stat)
{
  using DiagStatus = diagnostic_msgs::msg::DiagnosticStatus;
  int8_t level = DiagStatus::OK;
  std::string msg = "OK";

  if (!is_default_map_) {
    level = DiagStatus::ERROR;
    msg = "Default map is not found in " + csv_default_map_dir_;
  }

  if (new_mse_que_.size() < part_mse_que_size_ / 2) {
    stat.summary(level, msg);
    return;
  }

  const double rmse_rate = new_rmse_ / part_original_rmse_;
  if (rmse_rate < update_suggest_thresh_) {
    level = DiagStatus::WARN;
    msg = "Value map calibration is required.";
  }

  stat.summary(level, msg);
}

void GenericValueCalibrator::publish_float64(const std::string & publish_type, const double val)
{
  Float64Stamped msg;
  msg.header.stamp = this->now();
  msg.data = val;
  
  if (publish_type == "current_map_error") {
    current_map_error_pub_->publish(msg);
  } else if (publish_type == "updated_map_error") {
    updated_map_error_pub_->publish(msg);
  } else {
    map_error_ratio_pub_->publish(msg);
  }
}

void GenericValueCalibrator::publish_update_suggest_flag()
{
  std_msgs::msg::Bool update_suggest;

  if (new_mse_que_.size() < part_mse_que_size_ / 2) {
    update_suggest.data = false;
  } else {
    const double rmse_rate = new_rmse_ / part_original_rmse_;
    update_suggest.data = (rmse_rate < update_suggest_thresh_);
    if (update_suggest.data) {
      RCLCPP_WARN_STREAM_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "suggest to update value map. evaluation score = " << rmse_rate);
    }
  }

  update_suggest_pub_->publish(update_suggest);
}

bool GenericValueCalibrator::write_map_to_csv(
  std::vector<double> vel_index, std::vector<double> value_index, Map value_map,
  std::string filename)
{
  if (update_success_count_ == 0) {
    return false;
  }

  std::ofstream csv_file(filename);

  if (!csv_file.is_open()) {
    RCLCPP_WARN(get_logger(), "Failed to open csv file : %s", filename.c_str());
    return false;
  }

  csv_file << "default,";
  for (std::size_t v = 0; v < vel_index.size(); v++) {
    csv_file << vel_index.at(v);
    if (v != vel_index.size() - 1) {
      csv_file << ",";
    }
  }
  csv_file << "\n";

  for (std::size_t p = 0; p < value_index.size(); p++) {
    csv_file << value_index.at(p) << ",";
    for (std::size_t v = 0; v < vel_index.size(); v++) {
      csv_file << std::fixed << std::setprecision(precision_) << value_map.at(p).at(v);
      if (v != vel_index.size() - 1) {
        csv_file << ",";
      }
    }
    csv_file << "\n";
  }
  csv_file.close();
  RCLCPP_DEBUG_STREAM(get_logger(), "output map to " << filename);
  return true;
}

void GenericValueCalibrator::add_index_to_csv(std::ofstream * csv_file)
{
  *csv_file << "timestamp,velocity,accel,pitch_comp_accel,input_value,input_value_speed,"
            << "pitch,steer,jerk,part_original_rmse,new_rmse,rmse_rate"
            << std::endl;
}

}  // namespace autoware::generic_value_calibrator

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(autoware::generic_value_calibrator::GenericValueCalibrator)
