# 统计量更新修复总结

## 问题描述

用户发现 `data_num_` 变量没有正确更新，导致可视化中的数据计数显示不正确。

## 根本原因

在 `generic_value_calibrator_node.cpp` 中：
- `data_num_` 在初始化时被设置为 1
- 但在数据更新时（`update_each_val_offset` 和 `update_total_map_offset`），这些统计量从未被更新
- 这导致可视化始终显示计数为 1

相比之下，原始的 `accel_brake_map_calibrator_node.cpp` 正确地：
- 使用 `map_value_data_` 向量存储所有数据点
- 在每次更新时将新数据添加到向量中
- 使用 Welford 在线算法更新统计量（均值、方差、计数）

## 解决方案

### 1. 添加 Welford 在线算法更新

在 `update_each_val_offset` 中：
```cpp
/* Update statistics using Welford's online algorithm */
const double current_count = data_num_(value_index, vel_index);
const double pre_mean = data_mean_mat_(value_index, vel_index);
const double pre_variance = data_covariance_mat_(value_index, vel_index);

// Update mean
const double new_mean = (current_count * pre_mean + measured_acc) / (current_count + 1);

// Update variance
const double new_variance = 
  (current_count * (pre_variance + pre_mean * pre_mean) + measured_acc * measured_acc) / 
  (current_count + 1) - new_mean * new_mean;

// Update count
data_num_(value_index, vel_index) = current_count + 1;
data_mean_mat_(value_index, vel_index) = new_mean;
data_covariance_mat_(value_index, vel_index) = new_variance;
```

### 2. 创建独立的统计更新方法

添加了 `update_statistics` 方法，用于在 `UPDATE_OFFSET_TOTAL` 模式下也能跟踪数据覆盖：

```cpp
void GenericValueCalibrator::update_statistics(
  const int value_index, const int vel_index, const double measured_acc);
```

### 3. 在两种更新模式下都调用统计更新

```cpp
if (update_method_ == UPDATE_METHOD::UPDATE_OFFSET_EACH_CELL) {
  update_each_val_offset(value_index, vel_index, measured_acc, map_acc);
} else if (update_method_ == UPDATE_METHOD::UPDATE_OFFSET_TOTAL) {
  update_total_map_offset(measured_acc, map_acc);
  // Still update statistics for the current cell to track data coverage
  update_statistics(value_index, vel_index, measured_acc);
}
```

## Welford 在线算法原理

Welford 算法允许增量计算均值和方差，无需存储所有历史数据：

**更新均值**:
```
new_mean = (n * old_mean + new_value) / (n + 1)
```

**更新方差**:
```
new_var = (n * (old_var + old_mean²) + new_value²) / (n + 1) - new_mean²
```

其中 `n` 是当前数据点数量。

## 影响

修复后：
- ✅ `data_num_` 正确反映每个单元格收集的数据点数量
- ✅ `data_mean_mat_` 正确显示测量加速度的平均值
- ✅ `data_covariance_mat_` 正确显示数据的方差
- ✅ 可视化（OccupancyGrid 和 SVG plots）正确显示数据覆盖和统计信息
- ✅ `publish_count_map` 正确显示哪些单元格有数据，哪些没有

## 对比原始实现

| 方面 | 原始实现 | 之前的实现 | 修复后的实现 |
|------|---------|-----------|------------|
| 数据存储 | `map_value_data_` (vector) | 无 | 无 (使用统计量) |
| 统计更新 | Welford 算法 | ❌ 未更新 | ✅ Welford 算法 |
| 计数跟踪 | ✅ 每次添加数据时更新 | ❌ 始终为 1 | ✅ 每次添加数据时更新 |
| 均值跟踪 | ✅ 增量更新 | ❌ 初始值不变 | ✅ 增量更新 |
| 方差跟踪 | ✅ 增量更新 | ❌ 初始值不变 | ✅ 增量更新 |
| 内存效率 | 较低（存储所有点） | 高（仅存储统计量） | 高（仅存储统计量） |

## 验证

可以通过以下方式验证修复：

1. **运行校准器并查看 RViz**:
   ```bash
   ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml
   rviz2 -d occupancy.rviz
   ```
   - 查看 `~/debug/data_count_occ_map` topic
   - 应该看到单元格颜色从蓝色（少量数据）变为红色（大量数据）

2. **检查日志输出**:
   - 查看校准进度
   - 确认 `data_num_` 在增加

3. **查看 SVG 可视化**:
   ```bash
   ros2 run autoware_generic_value_calibrator generic_value_map_server.py
   ```
   - 生成的 `plot.svg` 应显示正确的数据点计数
   - "N pts" 标签应显示准确的数量

## 相关文件

修改的文件：
- `vehicle/autoware_generic_value_calibrator/src/generic_value_calibrator_node.cpp`
- `vehicle/autoware_generic_value_calibrator/include/autoware_generic_value_calibrator/generic_value_calibrator_node.hpp`

参考的原始实现：
- `vehicle/autoware_accel_brake_map_calibrator/src/accel_brake_map_calibrator_node.cpp` (lines 959-988)
