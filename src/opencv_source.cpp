#include "opencv_source.h"
#include <iostream>

OpenCVSource::OpenCVSource(int device_id, int width, int height, int fps)
    : device_id_(device_id), width_(width), height_(height), fps_(fps),
      is_camera_(true), is_initialized_(false) {
}

OpenCVSource::OpenCVSource(const std::string& source_path, int fps)
    : device_id_(-1), source_path_(source_path), fps_(fps),
      is_camera_(false), is_initialized_(false), width_(0), height_(0) {
}

OpenCVSource::~OpenCVSource() {
    release();
}

bool OpenCVSource::initialize() {
    try {
        if (is_camera_) {
            // Open camera device
            capture_.open(device_id_);
            if (!capture_.isOpened()) {
                std::cerr << "Failed to open camera device " << device_id_ << std::endl;
                return false;
            }
            
            // Set camera properties
            capture_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
            capture_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
            capture_.set(cv::CAP_PROP_FPS, fps_);
            
            // Get actual properties (may differ from requested)
            width_ = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
            height_ = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
            fps_ = static_cast<int>(capture_.get(cv::CAP_PROP_FPS));
            
            std::cout << "Camera " << device_id_ << " opened successfully" << std::endl;
        } else {
            // Open video file or stream
            capture_.open(source_path_);
            if (!capture_.isOpened()) {
                std::cerr << "Failed to open video source: " << source_path_ << std::endl;
                return false;
            }
            
            // Get video properties
            width_ = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
            height_ = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
            if (fps_ == 30) { // If default, get from video
                fps_ = static_cast<int>(capture_.get(cv::CAP_PROP_FPS));
            }
            
            std::cout << "Video source opened successfully: " << source_path_ << std::endl;
        }
        
        std::cout << "Resolution: " << width_ << "x" << height_ << " @ " << fps_ << " fps" << std::endl;
        
        is_initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in OpenCVSource::initialize: " << e.what() << std::endl;
        is_initialized_ = false;
        return false;
    }
}

bool OpenCVSource::getFrame(cv::Mat& frame) {
    if (!is_initialized_ || !capture_.isOpened()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(frame_mutex_);
    
    if (!capture_.read(frame)) {
        std::cerr << "Failed to read frame" << std::endl;
        return false;
    }
    
    if (frame.empty()) {
        return false;
    }
    
    return true;
}

void OpenCVSource::release() {
    if (is_initialized_ && capture_.isOpened()) {
        capture_.release();
        is_initialized_ = false;
        std::cout << "Video source released" << std::endl;
    }
}

std::string OpenCVSource::getName() const {
    if (is_camera_) {
        return "OpenCV Camera " + std::to_string(device_id_);
    } else {
        return "OpenCV Source: " + source_path_;
    }
}
