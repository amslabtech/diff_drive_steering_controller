cmake_minimum_required(VERSION 2.8.3)
project(diff_drive_steering_controller)

## Compile as C++11, supported in ROS Kinetic and newer
add_compile_options(-std=c++11)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  control_msgs
  controller_interface
  dynamic_reconfigure
  nav_msgs
  realtime_tools
  tf
  urdf
)
generate_dynamic_reconfigure_options(cfg/DiffDriveSteeringController.cfg)

catkin_package(
   INCLUDE_DIRS include
   LIBRARIES diff_drive_steering_controller
#  CATKIN_DEPENDS control_msgs controller_interface dynamic_reconfigure nav_msgs realtime_tools tf urdf
#  DEPENDS system_lib
)

include_directories(SYSTEM
  include
  ${catkin_INCLUDE_DIRS}
)

add_library(${PROJECT_NAME}
  src/diff_drive_steering_controller.cpp
  src/odometry.cpp
  src/speed_limiter.cpp
)
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})
add_dependencies(${PROJECT_NAME} ${${PROJECT_NAME}_EXPORTED_TARGETS} ${PROJECT_NAME}_gencfg)

install(DIRECTORY include/${PROJECT_NAME}/
    DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION})

# Install library
install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
)

