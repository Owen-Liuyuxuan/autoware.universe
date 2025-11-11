cmake_minimum_required(VERSION 3.14)
project(autoware_generic_value_converter)

find_package(autoware_cmake REQUIRED)
autoware_package()

ament_auto_add_library(${PROJECT_NAME}_lib SHARED
  src/generic_value_converter_node.cpp
  src/value_map.cpp
  src/csv_loader.cpp
)

rclcpp_components_register_node(${PROJECT_NAME}_lib
  PLUGIN "autoware::generic_value_converter::GenericValueConverterNode"
  EXECUTABLE ${PROJECT_NAME}_node
)

ament_auto_package(
  INSTALL_TO_SHARE
    config
    data
    launch
)
