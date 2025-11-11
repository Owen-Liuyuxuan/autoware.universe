# autoware_generic_value_calibrator

## 概述

`autoware_generic_value_calibrator` 是一个通用的值校准节点,用于自动校准任意float64输入值与车辆加速度之间的映射关系。它类似于 `autoware_accel_brake_map_calibrator`,但适用于任意的通用数值输入,而不仅仅是油门/刹车踏板。

## 功能特性

- **通用性**: 接收标准的 `Float64Stamped` 消息作为输入
- **自动校准**: 使用递归最小二乘法(RLS)自动更新速度-加速度映射表
- **数据过滤**: 自动过滤无效数据(如大转向角、大俯仰角、高加加速度等)
- **延迟补偿**: 考虑输入值到实际加速度的延迟
- **俯仰角补偿**: 从加速度中去除重力分量
- **实时评估**: 计算RMSE评估映射精度
- **CSV存储**: 将校准后的映射表保存为CSV格式

## 输入话题

| 话题名 | 消息类型 | 描述 |
|--------|---------|------|
| `~/input/velocity` | `autoware_vehicle_msgs::msg::VelocityReport` | 车辆速度信息 |
| `~/input/steer` | `autoware_vehicle_msgs::msg::SteeringReport` | 转向角信息 |
| `~/input/value` | `std_msgs::msg::Float64Stamped` | 通用float64输入值 |

## 输出话题

| 话题名 | 消息类型 | 描述 |
|--------|---------|------|
| `~/output/update_suggest` | `std_msgs::msg::Bool` | 建议更新映射表的标志 |
| `~/output/current_map_error` | `std_msgs::msg::Float64Stamped` | 当前映射表的误差 |
| `~/output/updated_map_error` | `std_msgs::msg::Float64Stamped` | 更新后映射表的误差 |
| `~/output/map_error_ratio` | `std_msgs::msg::Float64Stamped` | 误差比率 |

## 参数

### 系统参数

| 参数名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `update_hz` | double | 10.0 | 更新频率 |
| `update_method` | string | "update_offset_each_cell" | 更新算法 |
| `get_pitch_method` | string | "tf" | 俯仰角获取方法 |
| `csv_default_map_dir` | string | "" | 默认映射表目录 |
| `csv_calibrated_map_dir` | string | "" | 校准后映射表目录 |

### 算法参数

| 参数名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `initial_covariance` | double | 0.05 | 初始协方差 |
| `velocity_min_threshold` | double | 0.1 | 最小速度阈值 |
| `value_diff_threshold` | double | 0.03 | 值差异阈值 |
| `max_steer_threshold` | double | 0.2 | 最大转向角阈值 |
| `max_pitch_threshold` | double | 0.02 | 最大俯仰角阈值 |
| `max_jerk_threshold` | double | 0.7 | 最大加加速度阈值 |
| `value_to_accel_delay` | double | 0.3 | 输入值到加速度的延迟 |

## 使用方法

### 启动校准器

```bash
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml
```

### 校准流程

1. **启动校准器**: 加载默认映射表
2. **采集数据**: 驾驶车辆或发布测试数据
3. **实时过滤**: 只使用满足条件的稳定数据
4. **RLS更新**: 迭代修正映射表偏移量
5. **评估精度**: 计算RMSE判断是否需要保存
6. **保存映射**: 映射表自动保存到CSV文件

## 校准方法

### 数据预处理

在校准前,会自动过滤掉以下无效数据:
- 速度过低
- 转向角过大
- 俯仰角过大
- 加加速度过大
- 输入值变化速度过快

### 更新算法

#### UPDATE_OFFSET_EACH_CELL
使用接近每个网格的数据进行RLS更新。

**优点**: 每个点都使用接近的数据,精度高
**缺点**: 需要大量数据,校准时间较长

#### UPDATE_OFFSET_TOTAL
计算并应用全局偏移量到整个映射表。

**优点**: 简单快速
**缺点**: 精度可能较低

## CSV文件格式

映射表使用CSV格式存储,格式如下:

```csv
default,0.0,2.0,4.0,6.0,8.0,10.0,12.0,14.0,16.0,18.0,20.0
-1.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0
-0.8,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6
...
```

- 第一行: "default" + 速度索引(m/s)
- 第一列: 输入值索引
- 单元格: 对应的加速度值(m/s²)

## 与原始校准器的区别

| 特性 | accel_brake_map_calibrator | generic_value_calibrator |
|------|---------------------------|-------------------------|
| 输入类型 | 油门/刹车踏板值 | 任意float64值 |
| 输入消息 | `ActuationCommandStamped` | `Float64Stamped` |
| 映射表数量 | 2个(油门和刹车) | 1个 |
| 应用场景 | 车辆控制 | 通用映射校准 |
