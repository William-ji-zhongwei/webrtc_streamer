#ifndef H265_ENCODER_H
#define H265_ENCODER_H

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>

extern "C" {
#include <x265.h>
}

/**
 * @brief H.265/HEVC video encoder using x265
 */
class H265VideoEncoder {
public:
    H265VideoEncoder(int width, int height, int fps, int bitrate_kbps);
    ~H265VideoEncoder();
    
    bool initialize();
    bool encode(const cv::Mat& frame, std::vector<uint8_t>& encoded_data);
    void destroy();
    
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getFps() const { return fps_; }
    
private:
    int width_;
    int height_;
    int fps_;
    int bitrate_kbps_;
    
    x265_encoder* encoder_;
    x265_param* param_;
    x265_picture* pic_in_;
    
    int64_t pts_;
    bool initialized_;
};

#endif // H265_ENCODER_H
