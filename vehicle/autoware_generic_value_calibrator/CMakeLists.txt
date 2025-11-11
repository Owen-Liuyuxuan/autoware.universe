cmake_minimum_required(VERSION 3.14)
project(autoware_generic_value_calibrator)

find_package(autoware_cmake REQUIRED)
autoware_package()

find_package(Eigen3 REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# Generate custom messages
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Float64Stamped.msg"
  DEPENDENCIES std_msgs
)

ament_auto_add_library(${PROJECT_NAME} SHARED
  src/generic_value_calibrator_node.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${EIGEN3_INCLUDE_DIRS}
)

# Link generated messages
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(${PROJECT_NAME} "${cpp_typesupport_target}")

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
