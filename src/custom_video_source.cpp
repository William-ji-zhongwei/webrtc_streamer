#include "custom_video_source.h"
#include <api/video/i420_buffer.h>
#include <libyuv/convert.h>
#include <rtc_base/logging.h>

CustomVideoSource::CustomVideoSource() 
    : AdaptedVideoTrackSource(), timestamp_us_(0) {
}

void CustomVideoSource::PushFrame(const cv::Mat& frame) {
    if (frame.empty()) {
        return;
    }
    
    int width = frame.cols;
    int height = frame.rows;
    
    // Create I420 buffer
    rtc::scoped_refptr<webrtc::I420Buffer> buffer = 
        webrtc::I420Buffer::Create(width, height);
    
    // Convert BGR to I420
    if (frame.type() == CV_8UC3) {
        // BGR to I420 conversion using libyuv
        const int stride_bgr = frame.step;
        const uint8_t* src_bgr = frame.data;
        
        libyuv::RGB24ToI420(
            src_bgr, stride_bgr,
            buffer->MutableDataY(), buffer->StrideY(),
            buffer->MutableDataU(), buffer->StrideU(),
            buffer->MutableDataV(), buffer->StrideV(),
            width, height
        );
    } else if (frame.type() == CV_8UC1) {
        // Grayscale - just copy to Y plane and set U,V to 128
        memcpy(buffer->MutableDataY(), frame.data, width * height);
        memset(buffer->MutableDataU(), 128, width * height / 4);
        memset(buffer->MutableDataV(), 128, width * height / 4);
    } else {
        RTC_LOG(LS_ERROR) << "Unsupported frame format";
        return;
    }
    
    // Create VideoFrame
    timestamp_us_ += 33333;  // ~30fps (33.333ms per frame)
    
    webrtc::VideoFrame video_frame = 
        webrtc::VideoFrame::Builder()
            .set_video_frame_buffer(buffer)
            .set_timestamp_us(timestamp_us_)
            .build();
    
    // Push to WebRTC
    OnFrame(video_frame);
}
