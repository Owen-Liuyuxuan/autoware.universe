# Message Type Update - Using tier4_debug_msgs

## Summary

Both packages have been updated to use the existing `tier4_debug_msgs::msg::Float64Stamped` message type instead of creating custom message definitions or using non-existent `std_msgs` types.

## Changes Made

### 1. Package Dependencies

#### autoware_generic_value_calibrator/package.xml
**Added**:
```xml
<depend>tier4_debug_msgs</depend>
```

**Removed**:
- Custom message generation dependencies
- `rosidl_default_generators`
- `rosidl_default_runtime`
- `member_of_group` for rosidl_interface_packages

#### autoware_generic_value_converter/package.xml
**Added**:
```xml
<depend>tier4_debug_msgs</depend>
```

**Removed**:
- Custom message generation dependencies
- `rosidl_default_generators`
- `rosidl_default_runtime`
- `member_of_group` for rosidl_interface_packages

### 2. CMakeLists.txt Updates

#### Both Packages
**Removed**:
- `find_package(rosidl_default_generators REQUIRED)`
- `rosidl_generate_interfaces()` calls
- `rosidl_get_typesupport_target()` calls
- Custom message linking

**Result**: Clean, simple CMakeLists.txt without message generation complexity

### 3. Header File Updates

#### autoware_generic_value_calibrator/include/.../generic_value_calibrator_node.hpp
**Before**:
```cpp
#include "std_msgs/msg/float64_stamped.hpp"  // ❌ Doesn't exist
using std_msgs::msg::Float64Stamped;
```

**After**:
```cpp
#include "tier4_debug_msgs/msg/float64_stamped.hpp"  // ✅ Existing message
using tier4_debug_msgs::msg::Float64Stamped;
```

#### autoware_generic_value_converter/include/.../generic_value_converter_node.hpp
**Before**:
```cpp
#include "std_msgs/msg/float64_stamped.hpp"  // ❌ Doesn't exist
using Float64Stamped = std_msgs::msg::Float64Stamped;
```

**After**:
```cpp
#include "tier4_debug_msgs/msg/float64_stamped.hpp"  // ✅ Existing message
using tier4_debug_msgs::msg::Float64Stamped;
```

### 4. Documentation Updates

Both README.md files updated to reflect:
- Input/Output topics now show `tier4_debug_msgs::msg::Float64Stamped`
- Python example code uses correct import: `from tier4_debug_msgs.msg import Float64Stamped`

## Benefits of Using tier4_debug_msgs

1. ✅ **No Custom Messages**: Leverages existing Autoware infrastructure
2. ✅ **Simpler Build**: No message generation overhead
3. ✅ **Faster Compilation**: Removes rosidl dependencies and generation steps
4. ✅ **Better Integration**: Consistent with other Autoware debug messages
5. ✅ **Reduced Maintenance**: No custom message definitions to maintain
6. ✅ **Standard Practice**: Follows Autoware conventions for debug data

## Message Definition

The `tier4_debug_msgs::msg::Float64Stamped` message contains:

```msg
std_msgs/Header header
  builtin_interfaces/Time stamp
  string frame_id
float64 data
```

This is exactly what we need for generic float64 values with timestamps.

## Verification

To verify the changes compile correctly:

```bash
cd /workspace
colcon build --packages-select \
  autoware_generic_value_calibrator \
  autoware_generic_value_converter
```

## Files Modified

### autoware_generic_value_calibrator
1. ✅ `CMakeLists.txt` - Removed message generation
2. ✅ `package.xml` - Added tier4_debug_msgs dependency
3. ✅ `include/autoware_generic_value_calibrator/generic_value_calibrator_node.hpp` - Updated includes
4. ✅ `README.md` - Updated message type documentation

### autoware_generic_value_converter
1. ✅ `CMakeLists.txt` - Removed message generation
2. ✅ `package.xml` - Added tier4_debug_msgs dependency
3. ✅ `include/autoware_generic_value_converter/generic_value_converter_node.hpp` - Updated includes
4. ✅ `README.md` - Updated message type documentation

## Status

✅ **All changes complete and ready for compilation**

The packages now use the standard `tier4_debug_msgs::msg::Float64Stamped` message type consistently throughout.
