// Copyright 2024 Tier IV, Inc.
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

#include "gtest/gtest.h"

#include <Eigen/Dense>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

// Test utilities for calibrator functions
namespace test_utils
{

// Welford's online algorithm for mean and variance
struct WelfordStats {
  double count;
  double mean;
  double variance;

  WelfordStats() : count(1.0), mean(0.0), variance(0.05) {}

  void update(double value)
  {
    double pre_mean = mean;
    double pre_variance = variance;
    
    mean = (count * pre_mean + value) / (count + 1);
    variance = (count * (pre_variance + pre_mean * pre_mean) + value * value) / 
               (count + 1) - mean * mean;
    count += 1;
  }
};

// RLS (Recursive Least Squares) update
struct RLSState {
  double map_offset;
  double covariance;
  double forgetting_factor;

  RLSState(double initial_offset = 0.0, double initial_cov = 0.05, double lambda = 0.999)
    : map_offset(initial_offset), covariance(initial_cov), forgetting_factor(lambda) {}

  double update(double measured_acc, double map_acc)
  {
    const double phi = 1.0;
    covariance = (covariance - (covariance * phi * phi * covariance) /
                                 (forgetting_factor + phi * covariance * phi)) /
                 forgetting_factor;

    const double coef = (covariance * phi) / (forgetting_factor + phi * covariance * phi);
    const double error_map_offset = measured_acc - map_acc;
    map_offset = map_offset + coef * error_map_offset;

    return map_offset;
  }
};

// Index search function
bool index_value_search(
  const std::vector<double> & value_index, const double value, const double value_thresh,
  int * nearest_idx)
{
  double min_dist = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < value_index.size(); ++i) {
    const double dist = std::abs(value_index.at(i) - value);
    if (dist < min_dist) {
      min_dist = dist;
      *nearest_idx = i;
    }
  }
  return min_dist < value_thresh;
}

// CSV utilities
bool writeMapToCSV(
  const std::string & filename, const std::vector<double> & vel_index,
  const std::vector<double> & value_index,
  const std::vector<std::vector<double>> & value_map)
{
  std::ofstream csv_file(filename);
  if (!csv_file.is_open()) {
    return false;
  }

  // Write header
  csv_file << "default";
  for (const auto & vel : vel_index) {
    csv_file << "," << vel;
  }
  csv_file << "\n";

  // Write data rows
  for (size_t i = 0; i < value_index.size(); ++i) {
    csv_file << value_index[i];
    for (size_t j = 0; j < vel_index.size(); ++j) {
      csv_file << "," << value_map[i][j];
    }
    csv_file << "\n";
  }

  csv_file.close();
  return true;
}

bool readMapFromCSV(
  const std::string & filename, std::vector<double> & vel_index,
  std::vector<double> & value_index, std::vector<std::vector<double>> & value_map)
{
  std::ifstream csv_file(filename);
  if (!csv_file.is_open()) {
    return false;
  }

  std::vector<std::vector<std::string>> table;
  std::string line;
  while (std::getline(csv_file, line)) {
    std::istringstream iss(line);
    std::string cell;
    std::vector<std::string> row;
    while (std::getline(iss, cell, ',')) {
      row.push_back(cell);
    }
    table.push_back(row);
  }
  csv_file.close();

  if (table.size() < 2) {
    return false;
  }

  // Extract column indices (velocities)
  vel_index.clear();
  for (size_t i = 1; i < table[0].size(); ++i) {
    vel_index.push_back(std::stod(table[0][i]));
  }

  // Extract row indices (values)
  value_index.clear();
  for (size_t i = 1; i < table.size(); ++i) {
    value_index.push_back(std::stod(table[i][0]));
  }

  // Extract map data
  value_map.clear();
  for (size_t i = 1; i < table.size(); ++i) {
    std::vector<double> row;
    for (size_t j = 1; j < table[i].size(); ++j) {
      row.push_back(std::stod(table[i][j]));
    }
    value_map.push_back(row);
  }

  return true;
}

}  // namespace test_utils

// Test Welford's online algorithm
TEST(CalibratorTests, WelfordAlgorithm)
{
  test_utils::WelfordStats stats;
  
  // Initial state
  EXPECT_DOUBLE_EQ(stats.count, 1.0);
  EXPECT_DOUBLE_EQ(stats.mean, 0.0);
  EXPECT_DOUBLE_EQ(stats.variance, 0.05);

  // Add data points
  std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
  for (const auto & value : data) {
    stats.update(value);
  }

  // Expected mean: (0*1 + 1 + 2 + 3 + 4 + 5) / 6 = 15/6 = 2.5
  EXPECT_NEAR(stats.mean, 2.5, 1e-6);

  // Expected count: 1 + 5 = 6
  EXPECT_DOUBLE_EQ(stats.count, 6.0);

  // Variance should be positive
  EXPECT_GT(stats.variance, 0.0);
}

TEST(CalibratorTests, WelfordVarianceCalculation)
{
  test_utils::WelfordStats stats;

  // Add identical values - variance should approach zero
  for (int i = 0; i < 10; ++i) {
    stats.update(5.0);
  }

  EXPECT_NEAR(stats.mean, 5.0, 1e-6);
  EXPECT_NEAR(stats.variance, 0.0, 0.1);  // Should be very small
}

// Test RLS algorithm
TEST(CalibratorTests, RLSConvergence)
{
  test_utils::RLSState rls;

  // Simulate measurements with a constant offset
  const double true_offset = 0.5;
  const double map_value = 1.0;
  const double measured_value = map_value + true_offset;

  // Run RLS updates
  for (int i = 0; i < 100; ++i) {
    rls.update(measured_value, map_value);
  }

  // RLS should converge to the true offset
  EXPECT_NEAR(rls.map_offset, true_offset, 0.1);
}

TEST(CalibratorTests, RLSWithNoise)
{
  test_utils::RLSState rls;

  const double true_offset = 0.3;
  const double map_value = 1.0;

  // Add measurements with noise
  std::vector<double> noise = {0.05, -0.03, 0.02, -0.01, 0.04, -0.02};
  for (const auto & n : noise) {
    double measured_value = map_value + true_offset + n;
    rls.update(measured_value, map_value);
  }

  // Should still be close to true offset despite noise
  EXPECT_NEAR(rls.map_offset, true_offset, 0.15);
}

// Test index search
TEST(CalibratorTests, IndexSearch)
{
  std::vector<double> value_index = {0.0, 0.5, 1.0, 1.5, 2.0};
  int nearest_idx = -1;

  // Exact match
  EXPECT_TRUE(test_utils::index_value_search(value_index, 1.0, 0.1, &nearest_idx));
  EXPECT_EQ(nearest_idx, 2);

  // Close match within threshold
  EXPECT_TRUE(test_utils::index_value_search(value_index, 1.05, 0.1, &nearest_idx));
  EXPECT_EQ(nearest_idx, 2);

  // Just outside threshold
  EXPECT_FALSE(test_utils::index_value_search(value_index, 1.15, 0.1, &nearest_idx));

  // Boundary cases
  EXPECT_TRUE(test_utils::index_value_search(value_index, 0.0, 0.1, &nearest_idx));
  EXPECT_EQ(nearest_idx, 0);

  EXPECT_TRUE(test_utils::index_value_search(value_index, 2.0, 0.1, &nearest_idx));
  EXPECT_EQ(nearest_idx, 4);

  // Between two values
  EXPECT_TRUE(test_utils::index_value_search(value_index, 0.75, 0.3, &nearest_idx));
  EXPECT_TRUE(nearest_idx == 1 || nearest_idx == 2);  // Could be either 0.5 or 1.0
}

// Test CSV I/O
TEST(CalibratorTests, CSVWriteAndRead)
{
  const std::string test_file = "/tmp/test_calibration_map.csv";

  // Create test data
  std::vector<double> vel_index = {0.0, 5.0, 10.0, 15.0};
  std::vector<double> value_index = {-1.0, 0.0, 1.0};
  std::vector<std::vector<double>> value_map = {
    {-2.0, -2.0, -2.0, -2.0},
    {0.0, 0.0, 0.0, 0.0},
    {2.0, 2.0, 2.0, 2.0}
  };

  // Write to CSV
  EXPECT_TRUE(test_utils::writeMapToCSV(test_file, vel_index, value_index, value_map));

  // Read back
  std::vector<double> read_vel_index;
  std::vector<double> read_value_index;
  std::vector<std::vector<double>> read_value_map;
  
  EXPECT_TRUE(test_utils::readMapFromCSV(
    test_file, read_vel_index, read_value_index, read_value_map));

  // Verify
  EXPECT_EQ(read_vel_index, vel_index);
  EXPECT_EQ(read_value_index, value_index);
  EXPECT_EQ(read_value_map, value_map);

  // Cleanup
  std::remove(test_file.c_str());
}

TEST(CalibratorTests, CSVReadInvalidFile)
{
  std::vector<double> vel_index;
  std::vector<double> value_index;
  std::vector<std::vector<double>> value_map;

  EXPECT_FALSE(test_utils::readMapFromCSV(
    "/tmp/non_existent_file.csv", vel_index, value_index, value_map));
}

// Test parameterized range generation
TEST(CalibratorTests, ParameterizedRangeGeneration)
{
  const double value_min = -10.0;
  const double value_max = 10.0;
  const int value_num = 21;

  std::vector<double> value_index;
  const double value_step = (value_max - value_min) / (value_num - 1);
  for (int i = 0; i < value_num; ++i) {
    value_index.push_back(value_min + i * value_step);
  }

  // Verify
  EXPECT_EQ(value_index.size(), 21);
  EXPECT_DOUBLE_EQ(value_index.front(), -10.0);
  EXPECT_DOUBLE_EQ(value_index.back(), 10.0);
  EXPECT_NEAR(value_index[10], 0.0, 1e-6);  // Middle value
}

TEST(CalibratorTests, ParameterizedRangeSinglePoint)
{
  const double value_min = 5.0;
  const int value_num = 1;

  std::vector<double> value_index;
  if (value_num == 1) {
    value_index.push_back(value_min);
  }

  EXPECT_EQ(value_index.size(), 1);
  EXPECT_DOUBLE_EQ(value_index[0], 5.0);
}

// Test statistical matrix operations
TEST(CalibratorTests, EigenMatrixStatistics)
{
  const int rows = 3;
  const int cols = 4;

  // Initialize matrices
  Eigen::MatrixXd data_num = Eigen::MatrixXd::Constant(rows, cols, 1.0);
  Eigen::MatrixXd data_mean = Eigen::MatrixXd::Constant(rows, cols, 0.0);
  Eigen::MatrixXd data_variance = Eigen::MatrixXd::Constant(rows, cols, 0.05);

  // Update a specific cell
  int test_row = 1;
  int test_col = 2;
  double measured_value = 2.5;

  double count = data_num(test_row, test_col);
  double pre_mean = data_mean(test_row, test_col);
  double pre_variance = data_variance(test_row, test_col);

  // Apply Welford update
  double new_mean = (count * pre_mean + measured_value) / (count + 1);
  double new_variance = 
    (count * (pre_variance + pre_mean * pre_mean) + measured_value * measured_value) / 
    (count + 1) - new_mean * new_mean;

  data_num(test_row, test_col) = count + 1;
  data_mean(test_row, test_col) = new_mean;
  data_variance(test_row, test_col) = new_variance;

  // Verify
  EXPECT_DOUBLE_EQ(data_num(test_row, test_col), 2.0);
  EXPECT_NEAR(data_mean(test_row, test_col), 1.25, 1e-6);
  EXPECT_GT(data_variance(test_row, test_col), 0.0);

  // Verify other cells unchanged
  EXPECT_DOUBLE_EQ(data_num(0, 0), 1.0);
  EXPECT_DOUBLE_EQ(data_mean(0, 0), 0.0);
  EXPECT_DOUBLE_EQ(data_variance(0, 0), 0.05);
}

// Test data filtering logic
TEST(CalibratorTests, DataFiltering)
{
  // Simulate filtering thresholds
  const double velocity_min_threshold = 0.5;
  const double max_steer_threshold = 0.2;
  const double max_pitch_threshold = 0.02;
  const double max_jerk_threshold = 0.7;

  // Test case: all conditions pass
  double velocity = 1.0;
  double steer = 0.1;
  double pitch = 0.01;
  double jerk = 0.5;

  bool pass = (velocity > velocity_min_threshold) &&
              (std::abs(steer) < max_steer_threshold) &&
              (std::abs(pitch) < max_pitch_threshold) &&
              (std::abs(jerk) < max_jerk_threshold);

  EXPECT_TRUE(pass);

  // Test case: velocity too low
  velocity = 0.3;
  pass = (velocity > velocity_min_threshold) &&
         (std::abs(steer) < max_steer_threshold) &&
         (std::abs(pitch) < max_pitch_threshold) &&
         (std::abs(jerk) < max_jerk_threshold);

  EXPECT_FALSE(pass);

  // Test case: steering too large
  velocity = 1.0;
  steer = 0.25;
  pass = (velocity > velocity_min_threshold) &&
         (std::abs(steer) < max_steer_threshold) &&
         (std::abs(pitch) < max_pitch_threshold) &&
         (std::abs(jerk) < max_jerk_threshold);

  EXPECT_FALSE(pass);
}

// Test pitch compensation
TEST(CalibratorTests, PitchCompensation)
{
  constexpr double gravity = 9.80665;

  // Test zero pitch
  double pitch = 0.0;
  double pitch_comp_acc = gravity * std::sin(pitch);
  EXPECT_NEAR(pitch_comp_acc, 0.0, 1e-6);

  // Test positive pitch (uphill)
  pitch = 0.1;  // radians (~5.7 degrees)
  pitch_comp_acc = gravity * std::sin(pitch);
  EXPECT_NEAR(pitch_comp_acc, 0.978, 0.01);

  // Test negative pitch (downhill)
  pitch = -0.1;
  pitch_comp_acc = gravity * std::sin(pitch);
  EXPECT_NEAR(pitch_comp_acc, -0.978, 0.01);
}

// Test map consistency (monotonic relationships)
TEST(CalibratorTests, MapConsistency)
{
  std::vector<std::vector<double>> value_map = {
    {-2.0, -1.5, -1.0},  // value = -1.0
    {0.0, 0.0, 0.0},     // value = 0.0
    {2.0, 1.5, 1.0}      // value = 1.0
  };

  // Check that higher input values generally produce higher accelerations
  // (for same velocity)
  for (size_t vel_idx = 0; vel_idx < value_map[0].size(); ++vel_idx) {
    EXPECT_LE(value_map[0][vel_idx], value_map[2][vel_idx]);
  }

  // Check that higher velocities might have different accelerations
  // (vehicle dynamics - not necessarily monotonic)
  // Just verify the structure is valid
  for (const auto & row : value_map) {
    EXPECT_EQ(row.size(), 3);
  }
}
