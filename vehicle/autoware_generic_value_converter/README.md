# autoware_generic_value_converter

## 概述

`autoware_generic_value_converter` 是一个通用的值转换节点,用于将加速度命令转换为任意float64输出值。它使用校准好的速度-加速度映射表进行双线性插值,类似于 `autoware_raw_vehicle_cmd_converter`,但输出的是通用的float64值而不是特定的踏板命令。

## 功能特性

- **通用性**: 输出标准的 `Float64Stamped` 消息
- **双线性插值**: 使用速度和加速度查表得到输出值
- **CSV加载**: 从CSV文件加载映射表
- **实时转换**: 接收控制命令并实时转换为输出值
- **参数可配置**: 支持最大最小值限制

## 输入话题

| 话题名 | 消息类型 | 描述 |
|--------|---------|------|
| `~/input/control_cmd` | `autoware_control_msgs::msg::Control` | 控制命令(包含期望加速度) |
| `~/input/odometry` | `nav_msgs::msg::Odometry` | 里程计信息(用于获取当前速度) |

## 输出话题

| 话题名 | 消息类型 | 描述 |
|--------|---------|------|
| `~/output/value` | `std_msgs::msg::Float64Stamped` | 转换后的float64输出值 |

## 参数

| 参数名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `csv_path_value_map` | string | "" | 映射表CSV文件路径 |
| `max_value` | double | 1.0 | 最大输出值 |
| `min_value` | double | -1.0 | 最小输出值 |
| `convert_value_cmd` | bool | true | 是否进行转换 |
| `use_value_ff` | bool | true | 是否使用前馈(查表) |

## 使用方法

### 启动转换器

```bash
ros2 launch autoware_generic_value_converter generic_value_converter.launch.xml
```

### 指定自定义映射表

```bash
ros2 launch autoware_generic_value_converter generic_value_converter.launch.xml \
  csv_path_value_map:=/path/to/your/value_map.csv
```

## 工作原理

### 查表转换

转换器使用双线性插值从二维映射表中查找输出值:

1. **固定速度**: 对所有输入值,在当前速度下进行插值
2. **固定加速度**: 在插值后的加速度-输入值关系中查找对应的输入值

### 映射表格式

映射表使用CSV格式,与校准器输出的格式相同:

```csv
default,0.0,2.0,4.0,6.0,8.0,10.0,12.0,14.0,16.0,18.0,20.0
-1.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0,-2.0
-0.8,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6,-1.6
...
```

## 与原始转换器的区别

| 特性 | raw_vehicle_cmd_converter | generic_value_converter |
|------|--------------------------|------------------------|
| 输入类型 | 加速度命令 | 加速度命令 |
| 输出类型 | 油门/刹车踏板值 | 任意float64值 |
| 输出消息 | `ActuationCommandStamped` | `Float64Stamped` |
| 映射表数量 | 2个(油门和刹车) | 1个 |
| 应用场景 | 车辆控制 | 通用值映射 |

## 示例应用场景

1. **自定义执行器控制**: 控制非标准执行器,需要将加速度映射到自定义控制值
2. **仿真环境**: 在仿真中需要将加速度转换为仿真器特定的输入格式
3. **测试和开发**: 用于测试控制算法,将加速度命令转换为可视化或记录的数值
4. **传感器融合**: 将加速度映射到其他传感器的输入范围

## 集成示例

### 与校准器配合使用

```bash
# 1. 首先运行校准器采集数据并生成映射表
ros2 launch autoware_generic_value_calibrator generic_value_calibrator.launch.xml

# 2. 校准完成后,使用生成的映射表运行转换器
ros2 launch autoware_generic_value_converter generic_value_converter.launch.xml \
  csv_path_value_map:=~/autoware_map_calibration/value_map.csv
```

### 自定义节点订阅输出

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
