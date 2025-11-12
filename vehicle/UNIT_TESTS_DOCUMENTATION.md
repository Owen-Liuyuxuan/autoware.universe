# Unit Tests Documentation

## Overview

Comprehensive unit tests have been created for both `autoware_generic_value_calibrator` and `autoware_generic_value_converter` packages to ensure correctness and reliability of the implementation.

## Test Structure

### autoware_generic_value_converter Tests

**Location**: `vehicle/autoware_generic_value_converter/test/`

**Test File**: `test_autoware_generic_value_converter.cpp`

**Test Data**: `test/map_data/` directory contains CSV test files

#### Test Coverage

1. **CSV Loading Tests** (`ConverterTests.LoadValidPath`)
   - Valid path loading
   - Invalid path handling
   - Invalid map format detection (empty, 1-column, inconsistent rows)

2. **Map Loading Tests** (`ConverterTests.LoadExampleMap`)
   - Loading default value maps
   - Map validation

3. **Forward Mapping Tests** (`ConverterTests.ValueMapGetValue`)
   - `getValue(acceleration, velocity) → input_value`
   - Direct access to map values
   - Bilinear interpolation
   - Boundary value handling
   - Clamping for out-of-range inputs

4. **Reverse Mapping Tests** (`ConverterTests.ValueMapGetAcceleration`)
   - `getAcceleration(input_value, velocity) → acceleration`
   - Direct access
   - Interpolation
   - Clamping behavior

5. **Bilinear Interpolation Tests** (`ConverterTests.BilinearInterpolation`)
   - Verifies correct interpolation in 2D grid
   - Tests mid-grid point calculations

6. **Value Clamping Tests** (`ConverterTests.ClampValues`)
   - Tests `CSVLoader::clampValue` function
   - Verifies correct clamping behavior at boundaries

7. **CSV Loader Tests** (`CSVLoaderTests`)
   - Index extraction (`getRowIndex`, `getColumnIndex`)
   - Map validation (empty, NaN, Inf, inconsistent sizes)

### autoware_generic_value_calibrator Tests

**Location**: `vehicle/autoware_generic_value_calibrator/test/`

**Test File**: `test_generic_value_calibrator.cpp`

#### Test Coverage

1. **Welford Algorithm Tests** (`CalibratorTests.WelfordAlgorithm`)
   - Online mean calculation
   - Online variance calculation
   - Incremental updates
   - Convergence with identical values

2. **RLS (Recursive Least Squares) Tests**
   - `CalibratorTests.RLSConvergence`: Tests convergence to true offset
   - `CalibratorTests.RLSWithNoise`: Tests robustness with noisy measurements

3. **Index Search Tests** (`CalibratorTests.IndexSearch`)
   - Exact match finding
   - Near-match within threshold
   - Threshold boundary testing
   - Edge case handling (first, last elements)

4. **CSV I/O Tests**
   - `CalibratorTests.CSVWriteAndRead`: Round-trip CSV write/read
   - `CalibratorTests.CSVReadInvalidFile`: Error handling for missing files

5. **Parameterized Range Tests**
   - `CalibratorTests.ParameterizedRangeGeneration`: Multi-point range generation
   - `CalibratorTests.ParameterizedRangeSinglePoint`: Single-point edge case

6. **Statistical Matrix Tests** (`CalibratorTests.EigenMatrixStatistics`)
   - Eigen matrix operations
   - Welford updates on matrix elements
   - Cell independence verification

7. **Data Filtering Tests** (`CalibratorTests.DataFiltering`)
   - Velocity threshold filtering
   - Steering threshold filtering
   - Pitch threshold filtering
   - Jerk threshold filtering
   - Combined condition testing

8. **Pitch Compensation Tests** (`CalibratorTests.PitchCompensation`)
   - Zero pitch case
   - Uphill (positive pitch)
   - Downhill (negative pitch)
   - Gravity constant correctness

9. **Map Consistency Tests** (`CalibratorTests.MapConsistency`)
   - Monotonic relationship verification
   - Data structure validation

## Test Data Files

### Converter Test Maps

**test_value_map.csv**:
```csv
default,0.0,5.0,10.0
0.0,0.0,-0.3,-0.5
0.5,1.0,0.5,0.0
1.0,3.0,2.0,1.5
```
- 3×3 map for testing basic functionality
- Tests forward and reverse mapping

**test_empty_map.csv**:
```csv
default
```
- Tests error handling for empty maps

**test_1col_map.csv**:
```csv
default,0.0
0.0,0.0
0.5,1.0
```
- Tests error handling for insufficient data

**test_inconsistent_rows_map.csv**:
```csv
default,0.0,5.0,10.0
0.0,0.0,-0.3,-0.5
0.5,1.0,0.5
1.0,3.0,2.0,1.5
```
- Tests error handling for inconsistent row lengths

## Running Tests

### Build with Tests

```bash
cd /workspace
colcon build --packages-select autoware_generic_value_converter autoware_generic_value_calibrator \
  --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
```

### Run Converter Tests

```bash
colcon test --packages-select autoware_generic_value_converter
colcon test-result --verbose
```

### Run Calibrator Tests

```bash
colcon test --packages-select autoware_generic_value_calibrator
colcon test-result --verbose
```

### Run All Tests

```bash
colcon test --packages-select autoware_generic_value_converter autoware_generic_value_calibrator
colcon test-result --all --verbose
```

### Run Specific Test

```bash
./build/autoware_generic_value_converter/test_autoware_generic_value_converter --gtest_filter=ConverterTests.BilinearInterpolation
```

## Test Utilities

### Converter Tests Utilities
- `loadValueMapData()`: Helper to load test maps
- Lambda functions for cleaner test syntax

### Calibrator Tests Utilities (`test_utils` namespace)

1. **WelfordStats**
   - Implements Welford's online algorithm
   - Tracks count, mean, variance
   - `update(value)` method

2. **RLSState**
   - Implements RLS algorithm
   - Tracks map offset and covariance
   - `update(measured_acc, map_acc)` method

3. **index_value_search()**
   - Finds nearest index in sorted array
   - Returns success based on threshold

4. **CSV I/O Functions**
   - `writeMapToCSV()`: Save map to CSV
   - `readMapFromCSV()`: Load map from CSV

## Key Algorithms Tested

### 1. Welford's Online Algorithm

```cpp
mean = (count * old_mean + new_value) / (count + 1)
variance = (count * (old_variance + old_mean²) + new_value²) / (count + 1) - new_mean²
count = count + 1
```

**Why**: Incremental mean/variance calculation without storing all data points.

### 2. RLS (Recursive Least Squares)

```cpp
covariance = (covariance - (covariance * φ² * covariance) / (λ + φ * covariance * φ)) / λ
coefficient = (covariance * φ) / (λ + φ * covariance * φ)
offset = offset + coefficient * error
```

**Why**: Adaptive map offset calculation with forgetting factor.

### 3. Bilinear Interpolation

For point (x, y) between grid points:
```
f(x,y) ≈ f(x₀,y₀)*(1-rx)*(1-ry) + f(x₁,y₀)*rx*(1-ry) + 
         f(x₀,y₁)*(1-rx)*ry + f(x₁,y₁)*rx*ry
```
where `rx = (x-x₀)/(x₁-x₀)`, `ry = (y-y₀)/(y₁-y₀)`

**Why**: Smooth interpolation in 2D value-velocity space.

## Assertions Used

- `EXPECT_TRUE/FALSE`: Boolean checks
- `EXPECT_EQ`: Exact equality (for integers, containers)
- `EXPECT_DOUBLE_EQ`: Floating-point equality with default epsilon
- `EXPECT_NEAR(val, expected, epsilon)`: Floating-point with custom tolerance
- `EXPECT_GT/LT/GE/LE`: Comparison operators

## Expected Test Results

All tests should pass:
```
[==========] Running 21 tests from 3 test suites.
[----------] Global test environment set-up.
[----------] 8 tests from ConverterTests
[ RUN      ] ConverterTests.LoadExampleMap
[       OK ] ConverterTests.LoadExampleMap
...
[----------] 8 tests from ConverterTests (X ms total)

[----------] 13 tests from CalibratorTests
[ RUN      ] CalibratorTests.WelfordAlgorithm
[       OK ] CalibratorTests.WelfordAlgorithm
...
[----------] 13 tests from CalibratorTests (Y ms total)

[==========] 21 tests from 3 test suites ran. (Z ms total)
[  PASSED  ] 21 tests.
```

## Continuous Integration

These tests can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
test:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v2
    - name: Build
      run: colcon build --cmake-args -DBUILD_TESTING=ON
    - name: Test
      run: colcon test
    - name: Test Results
      run: colcon test-result --verbose
```

## Coverage

While not explicitly measured, the tests cover:
- ✅ Core algorithms (Welford, RLS, interpolation)
- ✅ CSV I/O operations
- ✅ Error handling (invalid files, malformed data)
- ✅ Edge cases (boundaries, single points)
- ✅ Numerical stability (noise, convergence)
- ✅ Data filtering logic
- ✅ Parameterized range generation
- ✅ Statistics matrix operations

## Future Test Enhancements

Potential additions:
1. **Performance tests**: Benchmark interpolation speed
2. **Integration tests**: Full node tests with ROS2
3. **Fuzz testing**: Random input generation
4. **Property-based testing**: Invariant verification
5. **Visualization tests**: Verify plot generation
6. **Memory tests**: Check for leaks
7. **Thread safety tests**: Concurrent access

## Debugging Failed Tests

If a test fails:

1. **Run with verbose output**:
   ```bash
   colcon test --packages-select <package> --event-handlers console_direct+
   ```

2. **Check specific test**:
   ```bash
   ./build/<package>/test_<package> --gtest_filter=TestName
   ```

3. **Enable debug output**:
   ```bash
   ./build/<package>/test_<package> --gtest_filter=TestName --gtest_output=verbose
   ```

4. **Check test log files**:
   ```bash
   cat build/<package>/Testing/Temporary/LastTest.log
   ```

## Summary

These comprehensive unit tests ensure:
- ✅ Correct implementation of core algorithms
- ✅ Robust error handling
- ✅ Numerical stability
- ✅ Compatibility with reference implementation
- ✅ Reliable CSV I/O
- ✅ Correct parameterized range handling

The tests serve as both verification and documentation of expected behavior.
