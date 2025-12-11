#ifndef OPENCV_SOURCE_H
#define OPENCV_SOURCE_H

#include "video_source.h"
#include <mutex>

/**
 * @brief Video source implementation using OpenCV VideoCapture
 * 
 * Supports USB cameras, video files, and RTSP streams
 */
class OpenCVSource : public VideoSource {
public:
    /**
     * @brief Constructor for camera device
     * @param device_id Camera device ID (default: 0)
     * @param width Desired width (default: 640)
     * @param height Desired height (default: 480)
     * @param fps Desired frame rate (default: 30)
     */
    OpenCVSource(int device_id = 0, int width = 640, int height = 480, int fps = 30);

    /**
     * @brief Constructor for video file or stream
     * @param source_path Path to video file or RTSP URL
     * @param fps Frame rate (for files, default: 30)
     */
    OpenCVSource(const std::string& source_path, int fps = 30);

    ~OpenCVSource() override;

    bool initialize() override;
    bool getFrame(cv::Mat& frame) override;
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    int getFrameRate() const override { return fps_; }
    void release() override;
    std::string getName() const override;
    bool isReady() const override { return capture_.isOpened(); }

private:
    cv::VideoCapture capture_;
    int device_id_;
    std::string source_path_;
    int width_;
    int height_;
    int fps_;
    bool is_camera_;
    bool is_initialized_;
    std::mutex frame_mutex_;
};

#endif // OPENCV_SOURCE_H
