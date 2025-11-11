cmake_minimum_required(VERSION 3.14)
project(autoware_generic_value_calibrator)

find_package(autoware_cmake REQUIRED)
autoware_package()

find_package(Eigen3 REQUIRED)

ament_auto_add_library(${PROJECT_NAME} SHARED
  src/generic_value_calibrator_node.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${EIGEN3_INCLUDE_DIRS}
)

rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "autoware::generic_value_calibrator::GenericValueCalibrator"
  EXECUTABLE ${PROJECT_NAME}_node
)

ament_auto_package(
  INSTALL_TO_SHARE
    config
    data
    launch
)
