#ifndef CUSTOM_VIDEO_SOURCE_H
#define CUSTOM_VIDEO_SOURCE_H

#include <api/video/video_frame.h>
#include <api/video/video_source_interface.h>
#include <media/base/adapted_video_track_source.h>
#include <rtc_base/ref_counted_object.h>
#include <opencv2/opencv.hpp>
#include <memory>

/**
 * @brief Custom video source for WebRTC
 * Adapts OpenCV Mat frames to WebRTC video frames
 */
class CustomVideoSource : public rtc::AdaptedVideoTrackSource {
public:
    CustomVideoSource();
    ~CustomVideoSource() override = default;
    
    // Push a new frame to the source
    void PushFrame(const cv::Mat& frame);
    
    // AdaptedVideoTrackSource implementation
    bool is_screencast() const override { return false; }
    absl::optional<bool> needs_denoising() const override { return false; }
    
    webrtc::MediaSourceInterface::SourceState state() const override {
        return webrtc::MediaSourceInterface::kLive;
    }
    
    bool remote() const override { return false; }

private:
    int64_t timestamp_us_;
};

#endif // CUSTOM_VIDEO_SOURCE_H
