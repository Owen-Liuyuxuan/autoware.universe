# Float64Stamped Message Structure Fix

## Issue

The `tier4_debug_msgs::msg::Float64Stamped` message definition is:

```msg
builtin_interfaces/Time stamp
float64 data
```

**NOT** the standard stamped message with a header:

```msg
std_msgs/Header header  # ❌ This doesn't exist in Float64Stamped
float64 data
```

## Key Difference

| Field | Standard Stamped Messages | tier4_debug_msgs::Float64Stamped |
|-------|---------------------------|----------------------------------|
| Timestamp | `msg.header.stamp` | `msg.stamp` |
| Frame ID | `msg.header.frame_id` | ❌ Not available |
| Data | `msg.data` | `msg.data` |

## Fixes Applied

### 1. autoware_generic_value_calibrator

#### File: `src/generic_value_calibrator_node.cpp`

**Line 358** - `take_input_value()`:
```cpp
// Before:
input_value_ptr_ = std::make_shared<DataStamped>(msg->data, rclcpp::Time(msg->header.stamp));

// After:
input_value_ptr_ = std::make_shared<DataStamped>(msg->data, rclcpp::Time(msg->stamp));
```

**Line 690** - `publish_float64()`:
```cpp
// Before:
Float64Stamped msg;
msg.header.stamp = this->now();
msg.data = val;

// After:
Float64Stamped msg;
msg.stamp = this->now();
msg.data = val;
```

### 2. autoware_generic_value_converter

#### File: `src/generic_value_converter_node.cpp`

**Lines 120-121** - `publishOutputValue()`:
```cpp
// Before:
Float64Stamped output_msg;
output_msg.header.stamp = this->now();
output_msg.header.frame_id = "base_link";  // ❌ This field doesn't exist
output_msg.data = output_value;

// After:
Float64Stamped output_msg;
output_msg.stamp = this->now();
output_msg.data = output_value;
```

## Summary of Changes

| Location | Old Code | New Code |
|----------|----------|----------|
| Calibrator - take_input_value | `msg->header.stamp` | `msg->stamp` |
| Calibrator - publish_float64 | `msg.header.stamp = ...` | `msg.stamp = ...` |
| Converter - publishOutputValue | `output_msg.header.stamp = ...` | `output_msg.stamp = ...` |
| Converter - publishOutputValue | `output_msg.header.frame_id = "base_link"` | ❌ Removed (field doesn't exist) |

## Impact

✅ **Fixed**: All Float64Stamped message accesses now use the correct field structure
✅ **Removed**: Non-existent `frame_id` field references
✅ **Compatible**: Code now matches the actual message definition

## Note on Frame ID

Since `tier4_debug_msgs::msg::Float64Stamped` doesn't have a `frame_id` field, the messages are not explicitly tied to a coordinate frame. This is acceptable for debug/generic float64 values that don't represent spatial quantities.

If frame information is needed in the future, consider:
1. Using a different message type (e.g., `geometry_msgs::msg::PointStamped` if spatial)
2. Adding frame information in the topic name or node documentation
3. Creating a custom message with both stamp and frame_id if truly necessary

## Verification

After these fixes, the code should compile successfully:

```bash
cd /workspace
colcon build --packages-select \
  autoware_generic_value_calibrator \
  autoware_generic_value_converter
```

All message field accesses now correctly match the `tier4_debug_msgs::msg::Float64Stamped` definition.
