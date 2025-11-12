# 映射关系验证

## 问题确认

您提到：
> The visualized map has an X axis=velocity, Y axis=input value. This is wrong.  
> The calibration should produce a X axis=velocity, Y axis=acceleration, and the input value should be the inside the grids.

## 原始实现验证

### 原始 accel_brake_map_calibrator

**数据结构**:
```cpp
// 来自源代码
const double h = accel_map_value_.size();        // 行 = pedal (输入值)
const double w = accel_map_value_.at(0).size();  // 列 = velocity

// OccupancyGrid 设置
occ.info.height = h;   // Y轴 = pedal
occ.info.width = w;    // X轴 = velocity

// 数据填充
for (int i = 0; i < h; i++) {      // 遍历 pedal (行)
  for (int j = 0; j < w; j++) {    // 遍历 velocity (列)
    int_map_value[i * w + j] = normalize(accel_map_value_[i][j]);  // 填充加速度
  }
}
```

**结论**: 原始实现是：
- **X轴 (width) = velocity** ✓
- **Y轴 (height) = pedal (input_value)** ✓  
- **单元格值 = acceleration** ✓

### 当前 generic_value_calibrator 实现

**数据结构**:
```cpp
// 来自当前实现
const double h = value_map_.size();        // 行 = input_value
const double w = value_map_.at(0).size();  // 列 = velocity

// OccupancyGrid 设置
occ.info.height = h;   // Y轴 = input_value
occ.info.width = w;    // X轴 = velocity

// 数据填充
for (int i = 0; i < h; i++) {      // 遍历 input_value (行)
  for (int j = 0; j < w; j++) {    // 遍历 velocity (列)
    int_map_value[i * w + j] = normalize(value_map_[i][j]);  // 填充加速度
  }
}
```

**结论**: 当前实现与原始**完全一致** ✓

## CSV 格式验证

### 原始 accel_map.csv
```
default,     0,   10,   20,   30,   40, ...  ← 列 = velocity (km/h)
0.0,       0.0,  0.0,  0.0,  0.0,  0.0, ...  ← pedal=0.0 的加速度
0.1,       0.5,  0.4,  0.3,  0.2,  0.1, ...  ← pedal=0.1 的加速度
0.2,       1.0,  0.9,  0.8,  0.7,  0.6, ...  ← pedal=0.2 的加速度
```

**映射**: (pedal, velocity) → acceleration

### 当前 generic_value_map.csv
```
default,   0.0,  2.0,  4.0,  6.0,  8.0, ...  ← 列 = velocity (m/s)
-1.0,     -2.0, -2.0, -2.0, -2.0, -2.0, ...  ← value=-1.0 的加速度
0.0,       0.0,  0.0,  0.0,  0.0,  0.0, ...  ← value=0.0 的加速度
1.0,       2.0,  2.0,  2.0,  2.0,  2.0, ...  ← value=1.0 的加速度
```

**映射**: (input_value, velocity) → acceleration

**格式完全一致** ✓

## Converter 逻辑验证

### 原始 AccelMap::getThrottle
```cpp
bool AccelMap::getThrottle(const double acc, double vel, double & throttle) const
{
  // 1. 固定 velocity，提取该列的所有加速度值
  std::vector<double> interpolated_acc_vec;
  for (const auto & accelerations : accel_map_) {
    interpolated_acc_vec.push_back(lerp(vel_index_, accelerations, vel));
  }
  
  // 2. 在加速度数组中，查找对应的 throttle (反向查找)
  throttle = lerp(interpolated_acc_vec, throttle_index_, acc);
  return true;
}
```

### 当前 ValueMap::getValue
```cpp
bool ValueMap::getValue(const double acc, double vel, double & value) const
{
  // 1. 固定 velocity，提取该列的所有加速度值
  std::vector<double> interpolated_acc_vec;
  for (const auto & accelerations : value_map_) {
    interpolated_acc_vec.push_back(lerp(vel_index_, accelerations, vel));
  }
  
  // 2. 在加速度数组中，查找对应的 value (反向查找)
  value = lerp(interpolated_acc_vec, value_index_, acc);
  return true;
}
```

**逻辑完全一致** ✓

## 映射哲学 (Philosophy)

### 校准阶段：收集数据
```
数据采集: (velocity, input_value) → measured_acceleration
例如:     (10 m/s, 0.5)        → 1.2 m/s²
          (10 m/s, 0.8)        → 1.8 m/s²
```

### 校准阶段：建立映射
```
CSV 存储: 
         vel=10
value=0.5  1.2    ← 单元格 (value=0.5, vel=10) 存储加速度 1.2
value=0.8  1.8    ← 单元格 (value=0.8, vel=10) 存储加速度 1.8
```

### 使用阶段：反向查找
```
控制器输入: "我在 10 m/s 时想要 1.5 m/s² 的加速度"
Converter 查找:
  1. 提取 vel=10 列: [1.2, 1.8, ...]
  2. 插值查找: 哪个 value 对应 acc=1.5?
  3. 答案: value ≈ 0.65
```

**这是正确的工作流程！** ✓

## 您期望的可视化 (velocity, acceleration) → input_value

如果您想看到 (velocity, acceleration) → input_value 的可视化，那是**反向映射**的显示方式，需要：

### 选项 1: 重新组织 OccupancyGrid
```
当前:
  Y轴 = input_value
  X轴 = velocity
  值 = acceleration

期望:
  Y轴 = acceleration
  X轴 = velocity
  值 = input_value  ← 需要反向插值计算！
```

**问题**: 这需要为每个 (velocity, acceleration) 网格点计算对应的 input_value，计算成本高且可能不唯一（一个加速度可能对应多个输入值，或没有输入值）。

### 选项 2: 使用多子图 (Python 可视化)
```python
# 当前 generic_value_map_server.py 已实现
# 为每个速度创建一个子图
for vel_idx in range(len(VEL_LIST)):
    subplot(vel_idx)
    plot(value_list, acceleration_list)  # X=value, Y=acceleration
    xlabel("Input Value")
    ylabel("Acceleration")
    title(f"Velocity = {VEL_LIST[vel_idx]} m/s")
```

**这已经在 SVG 输出中实现了！** ✓

## 结论与建议

1. **当前 OccupancyGrid 实现是正确的**，与原始完全一致
2. **CSV 格式是正确的**，映射方向正确
3. **Converter 逻辑是正确的**，能正确执行反向查找

### 如果您想要不同的可视化：

**推荐方案**: 使用 Python SVG 输出
- 已实现
- 更直观
- 每个速度一个子图，显示 input_value vs acceleration
- 包含数据点、均值、标准差

**备选方案**: 添加反向映射 OccupancyGrid
- 需要额外开发
- 计算成本高
- 可能存在不唯一性问题

## 验证请求

请确认：

1. **您看到的问题**是什么？
   - OccupancyGrid 在 RViz 中显示不正确？
   - Python SVG 图表不正确？
   - CSV 文件格式不正确？

2. **您期望的行为**是什么？
   - 希望看到反向映射可视化？
   - 希望改变坐标轴方向？
   - 其他？

请提供更多详细信息，我可以针对性地修复！
