#ifndef H265_DECODER_H
#define H265_DECODER_H

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

/**
 * @brief H.265/HEVC video decoder using FFmpeg
 */
class H265VideoDecoder {
public:
    H265VideoDecoder();
    ~H265VideoDecoder();
    
    bool initialize();
    bool decode(const uint8_t* data, size_t size, cv::Mat& frame);
    void destroy();
    
private:
    AVCodec* codec_;
    AVCodecContext* codec_ctx_;
    AVFrame* frame_;
    AVPacket* packet_;
    SwsContext* sws_ctx_;
    
    bool initialized_;
};

#endif // H265_DECODER_H
