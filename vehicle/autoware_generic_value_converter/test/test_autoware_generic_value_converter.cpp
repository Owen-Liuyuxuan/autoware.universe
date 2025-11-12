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

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "autoware_generic_value_converter/csv_loader.hpp"
#include "autoware_generic_value_converter/value_map.hpp"
#include "gtest/gtest.h"

#include <cmath>
#include <vector>

/*
 * Value data: (vel, value -> acc)
 *         0.0,  5.0,  10.0  (vel)
 * 0.0,    0.0, -0.3,  -0.5
 * 0.5,    1.0,  0.5,   0.0
 * 1.0,    3.0,  2.0,   1.5
 * (value)
 */

using autoware::generic_value_converter::CSVLoader;
using autoware::generic_value_converter::ValueMap;

double epsilon = 1e-4;

const auto map_path =
  ament_index_cpp::get_package_share_directory("autoware_generic_value_converter") +
  "/test/map_data/";

bool loadValueMapData(ValueMap & value_map)
{
  return value_map.readValueMapFromCSV(map_path + "test_value_map.csv");
}

TEST(ConverterTests, LoadExampleMap)
{
  ValueMap value_map;
  const auto data_path =
    ament_index_cpp::get_package_share_directory("autoware_generic_value_converter") + "/data/";
  // Load default value map
  EXPECT_TRUE(value_map.readValueMapFromCSV(data_path + "value_map.csv", true));
}

TEST(ConverterTests, LoadValidPath)
{
  ValueMap value_map;

  // for valid path
  EXPECT_TRUE(loadValueMapData(value_map));

  // for invalid path
  EXPECT_FALSE(value_map.readValueMapFromCSV("invalid.csv", true));

  // for invalid maps
  EXPECT_FALSE(value_map.readValueMapFromCSV(map_path + "test_1col_map.csv", true));
  EXPECT_FALSE(value_map.readValueMapFromCSV(map_path + "test_inconsistent_rows_map.csv", true));
  EXPECT_FALSE(value_map.readValueMapFromCSV(map_path + "test_empty_map.csv", true));
}

TEST(ConverterTests, ValueMapGetValue)
{
  ValueMap value_map;
  loadValueMapData(value_map);

  const auto calcValue = [&](double acc, double vel) {
    double output = 0.0;
    value_map.getValue(acc, vel, output);
    return output;
  };

  // Verify indices
  std::vector<double> map_column_idx = {0.0, 5.0, 10.0};
  std::vector<double> map_raw_idx = {0.0, 0.5, 1.0};
  std::vector<std::vector<double>> map_value = {
    {0.0, -0.3, -0.5}, {1.0, 0.5, 0.0}, {3.0, 2.0, 1.5}};

  EXPECT_EQ(value_map.getVelIdx(), map_column_idx);
  EXPECT_EQ(value_map.getValueIdx(), map_raw_idx);
  EXPECT_EQ(value_map.getValueMap(), map_value);

  // case for max vel nominal acc
  EXPECT_DOUBLE_EQ(calcValue(0.0, 20.0), 0.5);

  // case for max vel max acc
  EXPECT_DOUBLE_EQ(calcValue(2.0, 5.0), 1.0);

  // case for direct access
  EXPECT_DOUBLE_EQ(calcValue(0.5, 5.0), 0.5);

  // case for interpolation
  EXPECT_DOUBLE_EQ(calcValue(2.0, 0.0), 0.75);

  // case for max value
  EXPECT_DOUBLE_EQ(calcValue(2.0, 10.0), 1.0);

  // case for min value (negative acc)
  EXPECT_DOUBLE_EQ(calcValue(-1.0, 10.0), 0.0);
}

TEST(ConverterTests, ValueMapGetAcceleration)
{
  ValueMap value_map;
  loadValueMapData(value_map);

  const auto calcAcceleration = [&](double value, double vel) {
    double output;
    value_map.getAcceleration(value, vel, output);
    return output;
  };

  // case for min vel max value
  EXPECT_DOUBLE_EQ(calcAcceleration(1.0, 0.0), 3.0);

  // case for max vel max value
  EXPECT_DOUBLE_EQ(calcAcceleration(2.0, 10.0), 1.5);

  // case for direct access
  EXPECT_DOUBLE_EQ(calcAcceleration(0.0, 10.0), -0.5);

  // case for interpolation
  EXPECT_DOUBLE_EQ(calcAcceleration(0.75, 5.0), 1.25);

  // case for clamping (value > max)
  EXPECT_DOUBLE_EQ(calcAcceleration(1.5, 5.0), 2.0);

  // case for clamping (value < min)
  EXPECT_DOUBLE_EQ(calcAcceleration(-0.5, 5.0), -0.3);
}

TEST(ConverterTests, BilinearInterpolation)
{
  ValueMap value_map;
  loadValueMapData(value_map);

  const auto calcAcceleration = [&](double value, double vel) {
    double output;
    value_map.getAcceleration(value, vel, output);
    return output;
  };

  // Test bilinear interpolation in the middle of the grid
  // value = 0.25 (between 0.0 and 0.5), vel = 2.5 (between 0.0 and 5.0)
  // Expected: interpolation between 0.0, -0.15 (vel=2.5, value=0), and 1.0, 0.75 (vel=2.5, value=0.5)
  // At vel=2.5: value=0.0 -> acc = (0.0 + (-0.3))/2 = -0.15
  //             value=0.5 -> acc = (1.0 + 0.5)/2 = 0.75
  // At value=0.25: acc = -0.15 + 0.25 * (0.75 - (-0.15)) = -0.15 + 0.25 * 0.9 = 0.075
  EXPECT_NEAR(calcAcceleration(0.25, 2.5), 0.075, epsilon);
}

TEST(ConverterTests, ClampValues)
{
  ValueMap value_map;
  loadValueMapData(value_map);

  // Test CSVLoader clamp function
  std::vector<double> ranges = {0.0, 5.0, 10.0};
  
  EXPECT_DOUBLE_EQ(CSVLoader::clampValue(-1.0, ranges, "test"), 0.0);
  EXPECT_DOUBLE_EQ(CSVLoader::clampValue(0.0, ranges, "test"), 0.0);
  EXPECT_DOUBLE_EQ(CSVLoader::clampValue(5.0, ranges, "test"), 5.0);
  EXPECT_DOUBLE_EQ(CSVLoader::clampValue(10.0, ranges, "test"), 10.0);
  EXPECT_DOUBLE_EQ(CSVLoader::clampValue(15.0, ranges, "test"), 10.0);
}

TEST(CSVLoaderTests, GetIndices)
{
  ValueMap value_map;
  loadValueMapData(value_map);

  // Verify correct extraction of indices
  std::vector<double> vel_idx = {0.0, 5.0, 10.0};
  std::vector<double> value_idx = {0.0, 0.5, 1.0};

  EXPECT_EQ(value_map.getVelIdx(), vel_idx);
  EXPECT_EQ(value_map.getValueIdx(), value_idx);
}

TEST(CSVLoaderTests, ValidateMap)
{
  // Valid map
  std::vector<std::vector<double>> valid_map = {{1.0, 2.0}, {3.0, 4.0}};
  EXPECT_TRUE(CSVLoader::validateMap(valid_map, false));

  // Empty map
  std::vector<std::vector<double>> empty_map;
  EXPECT_FALSE(CSVLoader::validateMap(empty_map, false));

  // Map with empty rows
  std::vector<std::vector<double>> empty_row_map = {{}};
  EXPECT_FALSE(CSVLoader::validateMap(empty_row_map, false));

  // Map with inconsistent column count
  std::vector<std::vector<double>> inconsistent_map = {{1.0, 2.0}, {3.0}};
  EXPECT_FALSE(CSVLoader::validateMap(inconsistent_map, false));

  // Map with NaN
  std::vector<std::vector<double>> nan_map = {{1.0, std::nan("")}, {3.0, 4.0}};
  EXPECT_FALSE(CSVLoader::validateMap(nan_map, false));

  // Map with Inf
  std::vector<std::vector<double>> inf_map = {{1.0, std::numeric_limits<double>::infinity()}, {3.0, 4.0}};
  EXPECT_FALSE(CSVLoader::validateMap(inf_map, false));
}
