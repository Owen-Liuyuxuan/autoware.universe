# autoware_generic_value_converter

## Overview

`autoware_generic_value_converter` is a generic value conversion node that converts acceleration commands to arbitrary float64 output values. It uses bilinear interpolation on a calibrated velocity-acceleration mapping table, similar to `autoware_raw_vehicle_cmd_converter`, but outputs generic float64 values instead of specific pedal commands.

## Features

- **Generic**: Outputs standard `Float64Stamped` messages
- **Bilinear Interpolation**: Uses velocity and acceleration for table lookup to obtain output values
- **CSV Loading**: Loads mapping table from CSV file
- **Real-time Conversion**: Receives control commands and converts them to output values in real-time
- **Configurable Parameters**: Supports maximum and minimum value limits

## Input Topics

| Topic Name | Message Type | Description |
|------------|--------------|-------------|
| `~/input/control_cmd` | `autoware_control_msgs::msg::Control` | Control command (containing desired acceleration) |
| `~/input/odometry` | `nav_msgs::msg::Odometry` | Odometry information (for current velocity) |

## Output Topics

| Topic Name | Message Type | Description |
|------------|--------------|-------------|
| `~/output/value` | `std_msgs::msg::Float64Stamped` | Converted float64 output value |

## Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| `csv_path_value_map` | string | "" | Path to mapping CSV file |
| `max_value` | double | 1.0 | Maximum output value |
| `min_value` | double | -1.0 | Minimum output value |
| `convert_value_cmd` | bool | true | Whether to perform conversion |
| `use_value_ff` | bool | true | Whether to use feedforward (lookup table) |

## Usage

### Launch Converter

```bash
ros2 launch autoware_generic_value_converter generic_value_converter.launch.xml
```

### Specify Custom Mapping

```bash
ros2 launch autoware_generic_value_converter generic_value_converter.launch.xml \
  csv_path_value_map:=/path/to/your/value_map.csv
```

## How It Works

### Table Lookup Conversion

The converter uses bilinear interpolation to find output values from a 2D mapping table:

1. **Fix Velocity**: For all input values, interpolate at current velocity
2. **Fix Acceleration**: Find corresponding input value in the interpolated acceleration-input relationship

### Mapping Format

Mapping is stored in CSV format, same as calibrator output:

```csv
default,0.0,2.0,4.0,6.0,8.0,10.0,12.0,14.0,16.0,18.0,20.0
-1.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0
-0.8,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6
...
```

## Comparison with Original Converter

| Feature | raw_vehicle_cmd_converter | generic_value_converter |
|---------|--------------------------|------------------------|
| Input Type | Acceleration command | Acceleration command |
| Output Type | Throttle/Brake pedal values | Arbitrary float64 values |
| Output Message | `ActuationCommandStamped` | `Float64Stamped` |
| Number of Maps | 2 (throttle and brake) | 1 |
| Use Case | Vehicle control | Generic value mapping |

## Example Use Cases

1. **Custom Actuator Control**: Control non-standard actuators requiring acceleration to be mapped to custom control values
2. **Simulation Environment**: Convert acceleration to simulator-specific input format
3. **Testing and Development**: Test control algorithms by converting acceleration commands to visualizable or recordable values
4. **Sensor Fusion**: Map acceleration to other sensor input ranges

## Integration Examples

### Using with Calibrator

```bash
# 1. First run the calibrator to collect data and generate mapping
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml

# 2. After calibration, run converter with generated mapping
ros2 launch autoware_generic_value_converter generic_value_converter.launch.xml \
  csv_path_value_map:=~/autoware_map_calibration/value_map.csv
```

### Custom Node Subscribing to Output

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import Float64Stamped

class ValueSubscriber(Node):
    def __init__(self):
        super().__init__('value_subscriber')
        self.subscription = self.create_subscription(
            Float64Stamped,
            '/generic/output/value',
            self.callback,
            10)

    def callback(self, msg):
        self.get_logger().info(f'Received value: {msg.data}')
```
