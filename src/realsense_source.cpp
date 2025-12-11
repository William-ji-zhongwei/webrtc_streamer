#ifdef ENABLE_REALSENSE

#include "realsense_source.h"
#include <iostream>

RealSenseSource::RealSenseSource(int width, int height, int fps, bool enable_depth)
    : width_(width), height_(height), fps_(fps), enable_depth_(enable_depth),
      is_initialized_(false) {
}

RealSenseSource::~RealSenseSource() {
    release();
}

bool RealSenseSource::initialize() {
    try {
        // Configure color stream
        cfg_.enable_stream(RS2_STREAM_COLOR, width_, height_, RS2_FORMAT_BGR8, fps_);
        
        // Optionally enable depth stream
        if (enable_depth_) {
            cfg_.enable_stream(RS2_STREAM_DEPTH, width_, height_, RS2_FORMAT_Z16, fps_);
        }

        // Start the pipeline
        pipe_.start(cfg_);
        
        // Wait for first frames to stabilize
        for (int i = 0; i < 30; i++) {
            pipe_.wait_for_frames();
        }
        
        is_initialized_ = true;
        std::cout << "RealSense camera initialized successfully" << std::endl;
        std::cout << "Resolution: " << width_ << "x" << height_ << " @ " << fps_ << " fps" << std::endl;
        
        return true;
    } catch (const rs2::error& e) {
        std::cerr << "RealSense error: " << e.what() << std::endl;
        is_initialized_ = false;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        is_initialized_ = false;
        return false;
    }
}

bool RealSenseSource::getFrame(cv::Mat& frame) {
    if (!is_initialized_) {
        return false;
    }

    try {
        rs2::frameset frames = pipe_.wait_for_frames();
        rs2::frame color_frame = frames.get_color_frame();
        
        if (!color_frame) {
            return false;
        }

        // Create OpenCV Mat from RealSense frame
        const int w = color_frame.as<rs2::video_frame>().get_width();
        const int h = color_frame.as<rs2::video_frame>().get_height();
        
        cv::Mat temp(cv::Size(w, h), CV_8UC3, (void*)color_frame.get_data(), cv::Mat::AUTO_STEP);
        
        std::lock_guard<std::mutex> lock(frame_mutex_);
        frame = temp.clone();
        last_color_frame_ = frame.clone();
        
        // Handle depth if enabled
        if (enable_depth_) {
            rs2::frame depth = frames.get_depth_frame();
            if (depth) {
                rs2::frame colored_depth = color_map_.colorize(depth);
                cv::Mat depth_mat(cv::Size(w, h), CV_8UC3, 
                                (void*)colored_depth.get_data(), cv::Mat::AUTO_STEP);
                last_depth_frame_ = depth_mat.clone();
            }
        }
        
        return true;
    } catch (const rs2::error& e) {
        std::cerr << "RealSense error in getFrame: " << e.what() << std::endl;
        return false;
    }
}

bool RealSenseSource::getDepthFrame(cv::Mat& depth_frame) {
    if (!is_initialized_ || !enable_depth_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (last_depth_frame_.empty()) {
        return false;
    }
    
    depth_frame = last_depth_frame_.clone();
    return true;
}

void RealSenseSource::release() {
    if (is_initialized_) {
        try {
            pipe_.stop();
            is_initialized_ = false;
            std::cout << "RealSense camera released" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error releasing RealSense: " << e.what() << std::endl;
        }
    }
}

#endif // ENABLE_REALSENSE
