cmake_minimum_required(VERSION 3.10)
project(webrtc_streamer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(ENABLE_REALSENSE "Enable Intel RealSense camera support" ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# OpenCV (required)
find_package(OpenCV REQUIRED)

# RealSense (optional)
if(ENABLE_REALSENSE)
    find_package(realsense2)
    if(realsense2_FOUND)
        message(STATUS "Found RealSense: ${realsense2_VERSION}")
        add_definitions(-DENABLE_REALSENSE)
        set(REALSENSE_LIBS ${realsense2_LIBRARY})
    else()
        message(WARNING "RealSense not found, building without RealSense support")
        set(ENABLE_REALSENSE OFF)
        set(REALSENSE_LIBS "")
    endif()
else()
    message(STATUS "RealSense support disabled")
    set(REALSENSE_LIBS "")
endif()

# WebRTC (You may need to adjust the path to your WebRTC installation)
set(WEBRTC_ROOT_DIR "/opt/webrtc" CACHE PATH "WebRTC root directory")
if(EXISTS ${WEBRTC_ROOT_DIR})
    include_directories(${WEBRTC_ROOT_DIR}/include)
    link_directories(${WEBRTC_ROOT_DIR}/lib)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/video_source.cpp
    src/opencv_source.cpp
    src/webrtc_client.cpp
    src/config_parser.cpp
)

# Add RealSense source only if enabled
if(ENABLE_REALSENSE AND realsense2_FOUND)
    list(APPEND SOURCES src/realsense_source.cpp)
endif()

# Executable
add_executable(webrtc_streamer ${SOURCES})

# Link libraries
target_link_libraries(webrtc_streamer
    ${OpenCV_LIBS}
    ${REALSENSE_LIBS}
    pthread
    # WebRTC libraries (adjust based on your WebRTC build)
    # webrtc
)

# Installation
install(TARGETS webrtc_streamer DESTINATION bin)
