cmake_minimum_required(VERSION 3.16)
project(focus_monitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCV REQUIRED)

add_executable(focus_monitor src/main.cpp)
target_include_directories(focus_monitor PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(focus_monitor PRIVATE ${OpenCV_LIBS})
