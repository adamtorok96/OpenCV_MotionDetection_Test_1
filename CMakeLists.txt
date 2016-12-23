cmake_minimum_required(VERSION 3.6)
project(opencv_test_1)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp)

find_package(X11 REQUIRED)
find_package(OpenCV REQUIRED)

message(STATUS "X11_FOUND = ${X11_FOUND}")
message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")
message(STATUS "OpenCV_FOUND = ${OpenCV_FOUND}")

include_directories(${X11_INCLUDE_DIR})

link_directories(${X11_LIBRARIES})

add_executable(opencv_test_1 ${SOURCE_FILES})

target_link_libraries(opencv_test_1 ${X11_LIBRARIES})
target_link_libraries(opencv_test_1 ${OpenCV_LIBS} )