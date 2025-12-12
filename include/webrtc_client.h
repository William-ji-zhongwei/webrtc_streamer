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

// WebRTC headers
#include "api/scoped_refptr.h"
#include "api/peer_connection_interface.h"
#include "api/media_stream_interface.h"
#include "api/data_channel_interface.h"
#include "api/jsep.h"

class PeerConnectionObserver;
class CreateSessionDescriptionObserver;
class SetSessionDescriptionObserver;
class CustomVideoSource;

/**
 * @brief WebRTC client with native API and H.265 support
 */
class WebRTCClient {
public:
    WebRTCClient(std::shared_ptr<VideoSource> video_source,
                 const WebRTCConfig& webrtc_config);
    ~WebRTCClient();

    bool initialize();
    bool start();
    void stop();
    bool isStreaming() const { return is_streaming_; }
    
    // Callbacks from observers
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
    void OnConnectionChange(bool connected);
    void OnOfferCreated(webrtc::SessionDescriptionInterface* desc);
    void OnAnswerSet();

private:
    void streamingThread();
    void signalingThread();
    void captureAndEncodeFrames();
    
    bool createPeerConnection();
    bool addVideoTrack();
    void createOffer();
    void sendMessage(const std::string& message);
    std::string receiveMessage();
    
    std::shared_ptr<VideoSource> video_source_;
    WebRTCConfig webrtc_config_;
    
    std::atomic<bool> is_streaming_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> peer_connected_;
    
    std::thread streaming_thread_;
    std::thread signaling_thread_;
    
    // WebRTC components
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<CustomVideoSource> custom_video_source_;
    
    // Observers
    std::shared_ptr<PeerConnectionObserver> pc_observer_;
    
    // WebSocket connection
    int ws_socket_;
    std::mutex ws_mutex_;
    
    // Frame buffer
    std::queue<cv::Mat> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    static const size_t MAX_QUEUE_SIZE = 10;
    
    int frame_count_;
};

#endif // WEBRTC_CLIENT_H
