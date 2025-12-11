#include "h265_encoder.h"
#include <iostream>

H265VideoEncoder::H265VideoEncoder(int width, int height, int fps, int bitrate_kbps)
    : width_(width), height_(height), fps_(fps), bitrate_kbps_(bitrate_kbps),
      encoder_(nullptr), param_(nullptr), pic_in_(nullptr),
      pts_(0), initialized_(false) {
}

H265VideoEncoder::~H265VideoEncoder() {
    destroy();
}

bool H265VideoEncoder::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Allocate parameter structure
    param_ = x265_param_alloc();
    if (!param_) {
        std::cerr << "Failed to allocate x265 parameters" << std::endl;
        return false;
    }
    
    // Set default preset
    if (x265_param_default_preset(param_, "medium", "zerolatency") < 0) {
        std::cerr << "Failed to set x265 preset" << std::endl;
        x265_param_free(param_);
        return false;
    }
    
    // Configure parameters
    param_->sourceWidth = width_;
    param_->sourceHeight = height_;
    param_->fpsNum = fps_;
    param_->fpsDenom = 1;
    param_->rc.bitrate = bitrate_kbps_;
    param_->rc.rateControlMode = X265_RC_ABR;
    param_->bRepeatHeaders = 1;  // Repeat headers for each keyframe
    param_->internalCsp = X265_CSP_I420;
    
    // Low latency settings
    param_->bframes = 0;  // No B-frames for low latency
    param_->rc.vbvBufferSize = bitrate_kbps_;
    param_->rc.vbvMaxBitrate = bitrate_kbps_;
    param_->bIntraRefresh = 1;
    
    // Apply profile
    if (x265_param_apply_profile(param_, "main") < 0) {
        std::cerr << "Failed to apply x265 profile" << std::endl;
        x265_param_free(param_);
        return false;
    }
    
    // Create encoder
    encoder_ = x265_encoder_open(param_);
    if (!encoder_) {
        std::cerr << "Failed to open x265 encoder" << std::endl;
        x265_param_free(param_);
        return false;
    }
    
    // Allocate input picture
    pic_in_ = x265_picture_alloc();
    if (!pic_in_) {
        std::cerr << "Failed to allocate x265 picture" << std::endl;
        x265_encoder_close(encoder_);
        x265_param_free(param_);
        return false;
    }
    
    x265_picture_init(param_, pic_in_);
    
    initialized_ = true;
    std::cout << "H.265 encoder initialized: " 
              << width_ << "x" << height_ 
              << " @ " << fps_ << " fps, " 
              << bitrate_kbps_ << " kbps" << std::endl;
    
    return true;
}

bool H265VideoEncoder::encode(const cv::Mat& frame, std::vector<uint8_t>& encoded_data) {
    if (!initialized_) {
        std::cerr << "Encoder not initialized" << std::endl;
        return false;
    }
    
    if (frame.cols != width_ || frame.rows != height_) {
        std::cerr << "Frame size mismatch" << std::endl;
        return false;
    }
    
    // Convert BGR to YUV420
    cv::Mat yuv;
    cv::cvtColor(frame, yuv, cv::COLOR_BGR2YUV_I420);
    
    // Fill picture data
    int y_size = width_ * height_;
    int uv_size = y_size / 4;
    
    pic_in_->planes[0] = yuv.data;
    pic_in_->planes[1] = yuv.data + y_size;
    pic_in_->planes[2] = yuv.data + y_size + uv_size;
    
    pic_in_->stride[0] = width_;
    pic_in_->stride[1] = width_ / 2;
    pic_in_->stride[2] = width_ / 2;
    
    pic_in_->pts = pts_++;
    
    // Encode
    x265_nal* nals;
    uint32_t num_nals;
    x265_picture pic_out;
    
    int frame_size = x265_encoder_encode(encoder_, &nals, &num_nals, pic_in_, &pic_out);
    
    if (frame_size < 0) {
        std::cerr << "x265 encoding failed" << std::endl;
        return false;
    }
    
    if (frame_size > 0) {
        // Copy NAL units to output buffer
        encoded_data.clear();
        for (uint32_t i = 0; i < num_nals; i++) {
            encoded_data.insert(encoded_data.end(),
                              nals[i].payload,
                              nals[i].payload + nals[i].sizeBytes);
        }
        return true;
    }
    
    return false;  // No output (delayed)
}

void H265VideoEncoder::destroy() {
    if (pic_in_) {
        x265_picture_free(pic_in_);
        pic_in_ = nullptr;
    }
    
    if (encoder_) {
        x265_encoder_close(encoder_);
        encoder_ = nullptr;
    }
    
    if (param_) {
        x265_param_free(param_);
        param_ = nullptr;
    }
    
    initialized_ = false;
}
