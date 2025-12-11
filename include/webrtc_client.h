#ifndef WEBRTC_CLIENT_H
#define WEBRTC_CLIENT_H

#include "video_source.h"
#include "config_parser.h"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

/**
 * @brief WebRTC client for streaming video
 * 
 * This is a simplified implementation. For production use,
 * you should use the official WebRTC native API.
 */
class WebRTCClient {
public:
    /**
     * @brief Constructor
     * @param video_source Shared pointer to video source
     * @param webrtc_config WebRTC configuration (including ICE servers)
     */
    WebRTCClient(std::shared_ptr<VideoSource> video_source,
                 const WebRTCConfig& webrtc_config);

    ~WebRTCClient();

    /**
     * @brief Initialize the WebRTC connection
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Start streaming
     * @return true if successful
     */
    bool start();

    /**
     * @brief Stop streaming
     */
    void stop();

    /**
     * @brief Check if streaming is active
     * @return true if streaming
     */
    bool isStreaming() const { return is_streaming_; }

private:
    void streamingThread();
    void signalingThread();
    
    std::shared_ptr<VideoSource> video_source_;
    WebRTCConfig webrtc_config_;
    
    std::atomic<bool> is_streaming_;
    std::atomic<bool> should_stop_;
    
    std::thread streaming_thread_;
    std::thread signaling_thread_;
    
    // Frame buffer for encoding
    std::queue<cv::Mat> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    static const size_t MAX_QUEUE_SIZE = 10;
    
    int frame_count_;
};

#endif // WEBRTC_CLIENT_H
