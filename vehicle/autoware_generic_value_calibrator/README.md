# autoware_generic_value_calibrator

## Overview

`autoware_generic_value_calibrator` is a generic value calibration node that automatically calibrates the mapping relationship between arbitrary float64 input values and vehicle acceleration. It is similar to `autoware_accel_brake_map_calibrator`, but works with any generic numeric input rather than just throttle/brake pedal values.

## Features

- **Generic**: Accepts `tier4_debug_msgs::msg::Float64Stamped` messages as input
- **Automatic Calibration**: Uses Recursive Least Squares (RLS) algorithm to automatically update velocity-acceleration mapping
- **Data Filtering**: Automatically filters invalid data (large steering angles, pitch angles, jerk, etc.)
- **Delay Compensation**: Accounts for delay between input value and actual acceleration
- **Pitch Compensation**: Removes gravity component from acceleration measurements
- **Real-time Evaluation**: Calculates RMSE to evaluate mapping accuracy
- **CSV Storage**: Saves calibrated mapping to CSV format

## Input Topics

| Topic Name | Message Type | Description |
|------------|--------------|-------------|
| `~/input/velocity` | `autoware_vehicle_msgs::msg::VelocityReport` | Vehicle velocity information |
| `~/input/steer` | `autoware_vehicle_msgs::msg::SteeringReport` | Steering angle information |
| `~/input/value` | `tier4_debug_msgs::msg::Float64Stamped` | Generic float64 input value |

## Output Topics

| Topic Name | Message Type | Description |
|------------|--------------|-------------|
| `~/output/update_suggest` | `std_msgs::msg::Bool` | Flag suggesting map update |
| `~/output/current_map_error` | `tier4_debug_msgs::msg::Float64Stamped` | Current map error |
| `~/output/updated_map_error` | `tier4_debug_msgs::msg::Float64Stamped` | Updated map error |
| `~/output/map_error_ratio` | `tier4_debug_msgs::msg::Float64Stamped` | Error ratio |

## Parameters

### System Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| `update_hz` | double | 10.0 | Update frequency |
| `update_method` | string | "update_offset_each_cell" | Update algorithm |
| `get_pitch_method` | string | "tf" | Method to get pitch angle |
| `csv_default_map_dir` | string | "" | Default map directory |
| `csv_calibrated_map_dir` | string | "" | Calibrated map directory |

### Algorithm Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| `initial_covariance` | double | 0.05 | Initial covariance |
| `velocity_min_threshold` | double | 0.1 | Minimum velocity threshold |
| `value_diff_threshold` | double | 0.03 | Value difference threshold |
| `max_steer_threshold` | double | 0.2 | Maximum steering angle threshold |
| `max_pitch_threshold` | double | 0.02 | Maximum pitch angle threshold |
| `max_jerk_threshold` | double | 0.7 | Maximum jerk threshold |
| `value_to_accel_delay` | double | 0.3 | Delay from input value to acceleration |

## Usage

### Launch Calibrator

```bash
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml
```

### Calibration Workflow

1. **Launch Calibrator**: Load default mapping
2. **Collect Data**: Drive vehicle or publish test data
3. **Real-time Filtering**: Only use stable data meeting conditions
4. **RLS Update**: Iteratively correct mapping offsets
5. **Evaluate Accuracy**: Calculate RMSE to determine if saving is needed
6. **Save Mapping**: Map automatically saved to CSV file

## Calibration Methods

### Data Preprocessing

Before calibration, the following invalid data is automatically filtered:
- Low velocity
- Large steering angles
- Large pitch angles
- High jerk
- Fast input value changes

### Update Algorithms

#### UPDATE_OFFSET_EACH_CELL
Uses RLS update with data close to each grid cell.

**Advantages**: High accuracy using nearby data for each point
**Disadvantages**: Requires large amounts of data, longer calibration time

#### UPDATE_OFFSET_TOTAL
Calculates and applies global offset to entire mapping.

**Advantages**: Simple and fast
**Disadvantages**: Potentially lower accuracy

## CSV File Format

Mapping is stored in CSV format as follows:

```csv
default,0.0,2.0,4.0,6.0,8.0,10.0,12.0,14.0,16.0,18.0,20.0
-1.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0
-0.8,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6
...
```

- First row: "default" + velocity indices (m/s)
- First column: Input value indices
- Cells: Corresponding acceleration values (m/s²)

## Comparison with Original Calibrator

| Feature | accel_brake_map_calibrator | generic_value_calibrator |
|---------|---------------------------|-------------------------|
| Input Type | Throttle/Brake pedal values | Arbitrary float64 values |
| Input Message | `ActuationCommandStamped` | `Float64Stamped` |
| Number of Maps | 2 (throttle and brake) | 1 |
| Use Case | Vehicle control | Generic mapping calibration |
