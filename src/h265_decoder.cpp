#include "h265_decoder.h"
#include <iostream>

H265VideoDecoder::H265VideoDecoder()
    : codec_(nullptr), codec_ctx_(nullptr), frame_(nullptr),
      packet_(nullptr), sws_ctx_(nullptr), initialized_(false) {
}

H265VideoDecoder::~H265VideoDecoder() {
    destroy();
}

bool H265VideoDecoder::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Find H.265 decoder
    codec_ = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!codec_) {
        std::cerr << "H.265 codec not found" << std::endl;
        return false;
    }
    
    // Allocate codec context
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }
    
    // Open codec
    if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        avcodec_free_context(&codec_ctx_);
        return false;
    }
    
    // Allocate frame
    frame_ = av_frame_alloc();
    if (!frame_) {
        std::cerr << "Failed to allocate frame" << std::endl;
        avcodec_free_context(&codec_ctx_);
        return false;
    }
    
    // Allocate packet
    packet_ = av_packet_alloc();
    if (!packet_) {
        std::cerr << "Failed to allocate packet" << std::endl;
        av_frame_free(&frame_);
        avcodec_free_context(&codec_ctx_);
        return false;
    }
    
    initialized_ = true;
    std::cout << "H.265 decoder initialized" << std::endl;
    
    return true;
}

bool H265VideoDecoder::decode(const uint8_t* data, size_t size, cv::Mat& frame) {
    if (!initialized_) {
        std::cerr << "Decoder not initialized" << std::endl;
        return false;
    }
    
    // Set packet data
    packet_->data = const_cast<uint8_t*>(data);
    packet_->size = size;
    
    // Send packet to decoder
    int ret = avcodec_send_packet(codec_ctx_, packet_);
    if (ret < 0) {
        std::cerr << "Error sending packet to decoder" << std::endl;
        return false;
    }
    
    // Receive decoded frame
    ret = avcodec_receive_frame(codec_ctx_, frame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return false;  // Need more data
    } else if (ret < 0) {
        std::cerr << "Error receiving frame from decoder" << std::endl;
        return false;
    }
    
    // Initialize swscale context if needed
    if (!sws_ctx_) {
        sws_ctx_ = sws_getContext(
            frame_->width, frame_->height, static_cast<AVPixelFormat>(frame_->format),
            frame_->width, frame_->height, AV_PIX_FMT_BGR24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!sws_ctx_) {
            std::cerr << "Failed to create swscale context" << std::endl;
            return false;
        }
    }
    
    // Convert to BGR
    frame = cv::Mat(frame_->height, frame_->width, CV_8UC3);
    
    uint8_t* dest[1] = { frame.data };
    int dest_linesize[1] = { static_cast<int>(frame.step) };
    
    sws_scale(sws_ctx_,
             frame_->data, frame_->linesize,
             0, frame_->height,
             dest, dest_linesize);
    
    return true;
}

void H265VideoDecoder::destroy() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (packet_) {
        av_packet_free(&packet_);
    }
    
    if (frame_) {
        av_frame_free(&frame_);
    }
    
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    
    initialized_ = false;
}
