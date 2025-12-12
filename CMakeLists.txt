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

# FFmpeg (for H.265 decoding)
pkg_check_modules(LIBAV REQUIRED libavcodec libavformat libavutil libswscale)

# RealSense (optional)
if(ENABLE_REALSENSE)
    # Find Fast-DDS dependencies first (required by RealSense)
    find_package(fastcdr QUIET)
    find_package(fastrtps QUIET)
    
    find_package(realsense2 QUIET)
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

# WebRTC native API (社区预编译版本)
set(WEBRTC_ROOT_DIR "/opt/webrtc" CACHE PATH "WebRTC root directory")
if(EXISTS ${WEBRTC_ROOT_DIR})
    message(STATUS "Using prebuilt WebRTC from: ${WEBRTC_ROOT_DIR}")
    add_definitions(-DWEBRTC_POSIX)
    add_definitions(-DWEBRTC_LINUX)
    
    # 包含目录 - 适配预编译库的目录结构
    if(EXISTS ${WEBRTC_ROOT_DIR}/include)
        # 标准预编译库结构 (include/ 和 lib/)
        include_directories(
            ${WEBRTC_ROOT_DIR}/include
            ${WEBRTC_ROOT_DIR}/include/third_party/abseil-cpp
            ${WEBRTC_ROOT_DIR}/include/third_party/libyuv/include
        )
        link_directories(${WEBRTC_ROOT_DIR}/lib)
    else()
        # 源代码结构 (src/)
        include_directories(
            ${WEBRTC_ROOT_DIR}/src
            ${WEBRTC_ROOT_DIR}/src/third_party/abseil-cpp
            ${WEBRTC_ROOT_DIR}/src/third_party/libyuv/include
        )
        if(EXISTS ${WEBRTC_ROOT_DIR}/src/out/Release)
            link_directories(${WEBRTC_ROOT_DIR}/src/out/Release/obj)
        elseif(EXISTS ${WEBRTC_ROOT_DIR}/src/out/Default)
            link_directories(${WEBRTC_ROOT_DIR}/src/out/Default/obj)
        else()
            link_directories(${WEBRTC_ROOT_DIR}/lib)
        endif()
    endif()
    
    # WebRTC 库及其依赖
    set(WEBRTC_LIBS
        webrtc
        dl
        pthread
        rt
        X11
    )
else()
    message(WARNING "WebRTC not found at ${WEBRTC_ROOT_DIR}, using simplified implementation")
    message(WARNING "Please run: sudo ./scripts/install_webrtc.sh")
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
)

# Choose WebRTC implementation
list(APPEND SOURCES 
    src/webrtc_client.cpp
    src/custom_video_source.cpp
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
    ${LIBAV_LIBRARIES}
    ${WEBRTC_LIBS}
    pthread
    x265
    dl
)

# Installation
install(TARGETS webrtc_streamer DESTINATION bin)
