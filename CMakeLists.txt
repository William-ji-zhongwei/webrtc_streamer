cmake_minimum_required(VERSION 3.10)
project(webrtc_streamer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(ENABLE_REALSENSE "Enable Intel RealSense camera support" ON)
option(USE_NATIVE_WEBRTC "Use native WebRTC implementation" ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# OpenCV (required)
find_package(OpenCV REQUIRED)

# FFmpeg (for H.265 decoding)
pkg_check_modules(LIBAV REQUIRED libavcodec libavformat libavutil libswscale)

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

# WebRTC native API
set(WEBRTC_ROOT_DIR "/opt/webrtc" CACHE PATH "WebRTC root directory")
if(USE_NATIVE_WEBRTC)
    if(EXISTS ${WEBRTC_ROOT_DIR})
        message(STATUS "Using native WebRTC from: ${WEBRTC_ROOT_DIR}")
        add_definitions(-DUSE_NATIVE_WEBRTC)
        
        include_directories(
            ${WEBRTC_ROOT_DIR}/src
            ${WEBRTC_ROOT_DIR}/src/third_party/abseil-cpp
        )
        
        link_directories(${WEBRTC_ROOT_DIR}/src/out/Default/obj)
        
        set(WEBRTC_LIBS
            webrtc
        )
    else()
        message(WARNING "WebRTC not found at ${WEBRTC_ROOT_DIR}, using simplified implementation")
        set(USE_NATIVE_WEBRTC OFF)
        set(WEBRTC_LIBS "")
    endif()
else()
    message(STATUS "Using simplified WebRTC implementation")
    set(WEBRTC_LIBS "")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${LIBAV_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/video_source.cpp
    src/opencv_source.cpp
    src/config_parser.cpp
    src/h265_encoder.cpp
    src/h265_decoder.cpp
)

# Choose WebRTC implementation
if(USE_NATIVE_WEBRTC)
    list(APPEND SOURCES 
        src/webrtc_client_native.cpp
        src/custom_video_source.cpp
    )
else()
    list(APPEND SOURCES src/webrtc_client.cpp)
endif()

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
    ${LIBAV_LIBRARIES}
    ${WEBRTC_LIBS}
    pthread
    x265
    dl
)

# Installation
install(TARGETS webrtc_streamer DESTINATION bin)
