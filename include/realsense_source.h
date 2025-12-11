#ifndef REALSENSE_SOURCE_H
#define REALSENSE_SOURCE_H

#include "video_source.h"
#include <librealsense2/rs.hpp>
#include <mutex>

/**
 * @brief Video source implementation for Intel RealSense cameras (D455, etc.)
 * 
 * Supports color stream and optionally depth stream
 */
class RealSenseSource : public VideoSource {
public:
    /**
     * @brief Constructor
     * @param width Desired width (default: 640)
     * @param height Desired height (default: 480)
     * @param fps Desired frame rate (default: 30)
     * @param enable_depth Enable depth stream alongside color (default: false)
     */
    RealSenseSource(int width = 640, int height = 480, int fps = 30, bool enable_depth = false);
    
    ~RealSenseSource() override;

    bool initialize() override;
    bool getFrame(cv::Mat& frame) override;
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    int getFrameRate() const override { return fps_; }
    void release() override;
    std::string getName() const override { return "Intel RealSense"; }
    bool isReady() const override { return is_initialized_; }

    /**
     * @brief Get the depth frame (if enabled)
     * @param depth_frame Output depth frame
     * @return true if depth frame available
     */
    bool getDepthFrame(cv::Mat& depth_frame);

private:
    rs2::pipeline pipe_;
    rs2::config cfg_;
    rs2::colorizer color_map_;
    
    int width_;
    int height_;
    int fps_;
    bool enable_depth_;
    bool is_initialized_;
    
    std::mutex frame_mutex_;
    cv::Mat last_color_frame_;
    cv::Mat last_depth_frame_;
};

#endif // REALSENSE_SOURCE_H
