# Parameterized Input Value Range Implementation

## Overview

The `autoware_generic_value_calibrator` and `autoware_generic_value_converter` now support **fully parameterized input value and velocity ranges**. This makes each calibration map **self-contained** with its own scale and offset embedded in the CSV file.

## Why Parameterized Ranges?

### Problem
Previously, the input value range was hardcoded to -1.0 to 1.0, which required users to:
- Pre-scale their input values to this range
- Remember external scale/offset factors
- Manually coordinate between calibrator and converter

This was error-prone and made maps non-portable.

### Solution (Method 2 - Recommended)
- Define `value_min`, `value_max`, `value_num` as parameters
- Define `velocity_min`, `velocity_max`, `velocity_num` as parameters
- These parameters are embedded into the CSV file structure
- The converter automatically reads the ranges from the CSV

## Implementation Details

### 1. Calibrator (`autoware_generic_value_calibrator`)

**New Parameters** (in `generic_value_calibrator.param.yaml`):
```yaml
# Map index parameters
value_min: -1.0        # Minimum input value
value_max: 1.0         # Maximum input value
value_num: 11          # Number of points (includes endpoints)

velocity_min: 0.0      # Minimum velocity (m/s)
velocity_max: 20.0     # Maximum velocity (m/s)
velocity_num: 11       # Number of points
```

**Initialization Logic**:
1. If `csv_default_map_dir` is provided → Load indices from CSV file
2. If no CSV → Generate indices from parameters above
3. Create map: `value_map_[value_idx][velocity_idx] = acceleration`

**CSV Output Format**:
```csv
default,    0.0,  2.0,  4.0,  6.0, ...    ← Velocity indices (columns)
-1.0,      -2.0, -2.0, -2.0, -2.0, ...    ← Row: value=-1.0
 0.0,       0.0,  0.0,  0.0,  0.0, ...    ← Row: value=0.0
 1.0,       2.0,  2.0,  2.0,  2.0, ...    ← Row: value=1.0
```

The indices (first row and first column) are embedded in the CSV!

### 2. Converter (`autoware_generic_value_converter`)

**No Changes Needed!**

The converter already reads indices from CSV using `CSVLoader`:
```cpp
vel_index_ = CSVLoader::getColumnIndex(table);      // From first row
value_index_ = CSVLoader::getRowIndex(table);        // From first column
value_map_ = CSVLoader::getMap(table);               // Data cells
```

This means:
- ✅ Converter automatically adapts to any range
- ✅ No need to configure scale/offset separately
- ✅ Maps are portable and self-documenting

### 3. Visualization Scripts

**Updated** `config.py`:
- Added `load_indices_from_csv()` function
- Dynamically loads `VALUE_LIST` and `VEL_LIST` from map files
- Falls back to default if no CSV found

**Updated** `generic_value_map_server.py`:
- Loads indices from calibrated or default map at startup
- Generates plots with correct axis ranges automatically

## Usage Examples

### Example 1: Custom Control Signal (-10 to 10)

**config/my_calibration.param.yaml**:
```yaml
/**:
  ros__parameters:
    value_min: -10.0
    value_max: 10.0
    value_num: 21      # 21 points from -10 to 10
    velocity_min: 0.0
    velocity_max: 30.0
    velocity_num: 16   # High-speed vehicle
```

**Launch**:
```bash
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml \
  params_file:=my_calibration.param.yaml
```

**Result**:
- Calibrator creates map with input range [-10, 10]
- CSV file stores these indices
- Converter automatically uses [-10, 10] range when loading this CSV
- No manual scaling needed!

### Example 2: Normalized Input (0 to 1)

**Launch with parameters**:
```bash
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml \
  value_min:=0.0 value_max:=1.0 value_num:=11 \
  velocity_min:=0.0 velocity_max:=20.0 velocity_num:=11
```

### Example 3: Load Existing Map

If you have a pre-calibrated map:

```bash
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml \
  csv_default_map_dir:=/path/to/my_previous_calibration.csv
```

- The calibrator will load the indices from the CSV
- Parameters `value_min`, `value_max`, etc. are ignored
- Ensures consistency with previous calibration

## Benefits

### ✅ Self-Contained Maps
Each CSV file contains:
- Input value range
- Velocity range
- Acceleration mapping

No external documentation needed!

### ✅ No Manual Scaling
Before:
```python
# User had to do this manually
scaled_input = (raw_input - offset) / scale  # Scale to [-1, 1]
# Send scaled_input to calibrator
# ...later...
raw_output = (converter_output * scale) + offset  # Scale back
```

Now:
```python
# Just send raw values directly!
# Calibrator and converter handle everything
```

### ✅ Easy Migration
To change ranges:
1. Update parameters in `param.yaml`
2. Delete old map file
3. Restart calibration
4. New map automatically uses new range

### ✅ Backward Compatible
- Default parameters match original behavior
- Existing launch files continue to work
- Existing CSV files are still valid

## CSV File Structure

### Header Row (First Row)
```
default, vel_0, vel_1, vel_2, ...
```
- First cell: Vehicle/map name
- Remaining cells: Velocity values (m/s)

### Data Rows
```
value_i, acc_i0, acc_i1, acc_i2, ...
```
- First cell: Input value
- Remaining cells: Acceleration values (m/s²)

### Example
```csv
default,    0.0,  5.0, 10.0, 15.0, 20.0
-5.0,     -10.0,-10.0,-10.0,-10.0,-10.0
 0.0,       0.0,  0.0,  0.0,  0.0,  0.0
 5.0,      10.0, 10.0, 10.0, 10.0, 10.0
```

This map:
- Input range: [-5, 5]
- Velocity range: [0, 20] m/s
- Linear mapping: accel = 2 * input_value

## Technical Notes

### Index Generation
```cpp
// For value_num = N points from value_min to value_max
if (value_num == 1) {
  value_index = {value_min};
} else {
  double step = (value_max - value_min) / (value_num - 1);
  for (int i = 0; i < value_num; ++i) {
    value_index[i] = value_min + i * step;
  }
}
```

This ensures:
- Endpoints are exactly `value_min` and `value_max`
- Uniform spacing between points
- Total of `value_num` points (inclusive)

### Map Dimensions
- Map size = `value_num` × `velocity_num`
- Example: 21 values × 16 velocities = 336 calibration cells
- Larger maps = better resolution, but slower calibration

## Comparison

| Aspect | Method 1 (External Scale) | Method 2 (Parameterized) ✅ |
|--------|---------------------------|----------------------------|
| Map contains scale? | ❌ No | ✅ Yes (in indices) |
| User scaling needed? | ✅ Yes | ❌ No |
| Self-documenting? | ❌ No | ✅ Yes |
| Error-prone? | ✅ Yes | ❌ No |
| Portable? | ❌ No | ✅ Yes |
| Converter config? | ✅ Must match | ❌ Auto-detect |

## Migration Guide

### From Hardcoded Range
If you were using the old hardcoded [-1, 1] range:

**Before**:
```bash
# Input was always [-1, 1]
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml
```

**After** (same behavior):
```bash
# Explicit parameters (default values)
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml \
  value_min:=-1.0 value_max:=1.0 value_num:=11
```

### From Manual Scaling
If you were manually scaling inputs:

**Before**:
```python
SCALE = 10.0
OFFSET = 5.0
scaled = (raw - OFFSET) / SCALE  # → [-1, 1]
publisher.publish(Float64Stamped(data=scaled))
```

**After**:
```python
# Set parameters to match your raw range
# value_min: -5.0 (= OFFSET - SCALE)
# value_max: 15.0 (= OFFSET + SCALE)

# No scaling needed!
publisher.publish(Float64Stamped(data=raw))
```

## Summary

The parameterized range implementation makes the generic value calibrator truly flexible and self-contained. Users can now:

1. ✅ Calibrate any input range without manual scaling
2. ✅ Share calibration maps without documentation
3. ✅ Change ranges easily through parameters
4. ✅ Avoid scale/offset coordination errors

The converter automatically adapts to whatever range is in the CSV file, making the entire system robust and user-friendly.
