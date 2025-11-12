# 映射关系说明和澄清

## 当前实现分析

### CSV 格式（已验证正确）

**当前 generic_value_map.csv**:
```
default,    0.0,  2.0,  4.0,  6.0, ...    ← 列索引 = velocity (m/s)
-1.0,      -2.0, -2.0, -2.0, -2.0, ...    ← 行: input_value = -1.0 时的加速度
 0.0,       0.0,  0.0,  0.0,  0.0, ...    ← 行: input_value = 0.0 时的加速度
 1.0,       2.0,  2.0,  2.0,  2.0, ...    ← 行: input_value = 1.0 时的加速度
```

**这与原始 accel_map.csv 格式完全一致**:
```
default,    0,   10,   20,   30, ...      ← 列索引 = velocity (km/h)
0.0,       0.0,  0.0,  0.0,  0.0, ...     ← 行: throttle = 0.0 时的加速度
0.1,       0.5,  0.4,  0.3,  0.2, ...     ← 行: throttle = 0.1 时的加速度
0.2,       1.0,  0.9,  0.8,  0.7, ...     ← 行: throttle = 0.2 时的加速度
```

### 数据结构（已验证正确）

```cpp
// value_map_[row][column] = acceleration
// row = input_value_index  
// column = velocity_index
Map value_map_;  // std::vector<std::vector<double>>
```

### 校准阶段：Forward Mapping

**收集的数据**:
- 输入: (velocity, input_value, measured_acceleration)
- 建立映射: (velocity, input_value) → acceleration

**可视化方式 1（当前 OccupancyGrid）**:
```
Y轴
^
|  input_value_N  [acc] [acc] [acc] [acc] ...
|  input_value_2  [acc] [acc] [acc] [acc] ...
|  input_value_1  [acc] [acc] [acc] [acc] ...
|  input_value_0  [acc] [acc] [acc] [acc] ...
+-----------------------------------------> X轴
                 vel0 vel1 vel2 vel3 ...
```
- X轴 = velocity
- Y轴 = input_value  
- 单元格值/颜色 = acceleration

**这是正确的映射方向！**

### 使用阶段：Reverse Lookup

**generic_value_converter 的工作**:
```cpp
// 输入: desired_acceleration, current_velocity
// 查找: 在 current_velocity 列中，哪个 input_value 能产生 desired_acceleration
bool getValue(const double acc, double vel, double & value) const
{
  // 1. 提取 current_velocity 列的所有加速度值
  std::vector<double> interpolated_acc_vec;
  for (const auto & accelerations : value_map_) {
    interpolated_acc_vec.push_back(lerp(vel_index_, accelerations, vel));
  }
  
  // 2. 在加速度数组中查找对应的 input_value
  value = lerp(interpolated_acc_vec, value_index_, acc);
  return true;
}
```

## 用户期望的可视化

根据您的描述：
> X axis=velocity, Y axis=acceleration, and the input value should be the inside the grids

**可视化方式 2（反向映射）**:
```
Y轴 (acceleration)
^
|  acc_N  [val] [val] [val] [val] ...
|  acc_2  [val] [val] [val] [val] ...
|  acc_1  [val] [val] [val] [val] ...
|  acc_0  [val] [val] [val] [val] ...
+-----------------------------------------> X轴 (velocity)
             vel0 vel1 vel2 vel3 ...
```
- X轴 = velocity
- Y轴 = acceleration
- 单元格值/颜色 = input_value

**这是反向映射的可视化！**

## 两种可视化的区别

### 方式 1（当前实现）- Forward Map View
- **问题**: "给定速度和输入值，会产生什么加速度？"
- **用途**: 
  - 显示校准数据的原始结构
  - 检查哪些 (velocity, input_value) 组合有数据
  - 查看加速度分布

### 方式 2（用户期望）- Reverse Map View  
- **问题**: "给定速度和期望加速度，应该给什么输入值？"
- **用途**:
  - 更直观的控制视角
  - 直接看到控制策略
  - 检查输入值的连续性

## 原始 accel_brake_map_calibrator 的可视化

**view_pedal_accel_graph** 实际显示的是：
```python
# 为每个速度创建一个子图
# 在该子图中：
# X轴 = pedal (input_value)
# Y轴 = acceleration
plotter.plot(pedal_list[vel_idx], acc_list[vel_idx])
```

**这是 Forward Map 的切片视图**！
- 每个子图对应一个速度
- 在该速度下，显示 pedal → acceleration 的关系

## 问题诊断

### 问题 1: OccupancyGrid 方向是否正确？

**当前实现**:
```cpp
const double h = value_map_.size();         // value_index 数量
const double w = value_map_.at(0).size();   // velocity_index 数量
occ.info.height = h;  // Y轴 = input_value
occ.info.width = w;   // X轴 = velocity
```

**这与原始实现一致！** ✓

### 问题 2: 单元格值是否正确？

**当前实现**:
```cpp
for (int i = 0; i < h; i++) {      // 遍历 input_value
  for (int j = 0; j < w; j++) {    // 遍历 velocity
    const double value = value_map.at(i).at(j);  // 获取 acceleration
    int8_t int_value = normalize(value);  // 归一化到 0-100
    int_map_value[i * w + j] = int_value;
  }
}
```

**这是正确的！** ✓

### 问题 3: 是否需要反向映射可视化？

如果用户想要看 (velocity, acceleration) → input_value 的视图，我们需要：

1. **创建反向映射表**（计算密集）
2. **或者提供不同的可视化工具**（推荐）

## 建议方案

### 方案 A: 保持当前实现（推荐）
- 当前实现与原始代码完全一致
- 映射方向正确
- 可视化清晰显示数据覆盖情况

### 方案 B: 添加反向可视化
创建额外的工具显示反向映射：
```python
# 新脚本: view_inverse_map.py
# 输入: generic_value_map.csv
# 输出: 反向映射可视化
# X轴 = velocity
# Y轴 = acceleration
# 颜色 = input_value
```

### 方案 C: 使用 Python 可视化服务器的子图方式
当前的 `generic_value_map_server.py` 已经实现了类似原始的多子图可视化：
- 每个子图对应一个速度
- X轴 = input_value
- Y轴 = acceleration

**这才是最直观的可视化方式！**

## 验证请求

请确认您想要的是：

1. **保持当前 OccupancyGrid 实现**（与原始一致）？
2. **只使用 Python SVG 图表**（多子图，每个速度一个）？
3. **添加反向映射的 OccupancyGrid**（需要额外计算）？

或者，请提供：
- 您看到的当前可视化截图
- 您期望的可视化应该是什么样的

我可以根据您的具体需求调整可视化方式！
