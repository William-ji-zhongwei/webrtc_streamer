#include "webrtc_client.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

WebRTCClient::WebRTCClient(std::shared_ptr<VideoSource> video_source,
                           const WebRTCConfig& webrtc_config)
    : video_source_(video_source), webrtc_config_(webrtc_config),
      is_streaming_(false), should_stop_(false), frame_count_(0) {
}

WebRTCClient::~WebRTCClient() {
    stop();
}

bool WebRTCClient::initialize() {
    if (!video_source_ || !video_source_->isReady()) {
        std::cerr << "Video source is not ready" << std::endl;
        return false;
    }
    
    std::cout << "WebRTC client initialized" << std::endl;
    std::cout << "Server: " << webrtc_config_.server_ip << ":" << webrtc_config_.server_port << std::endl;
    std::cout << "Video source: " << video_source_->getName() << std::endl;
    
    std::cout << "\nICE Servers configuration:" << std::endl;
    for (size_t i = 0; i < webrtc_config_.ice_servers.size(); i++) {
        const auto& ice = webrtc_config_.ice_servers[i];
        std::cout << "  [" << i + 1 << "] ";
        for (size_t j = 0; j < ice.urls.size(); j++) {
            std::cout << ice.urls[j];
            if (j < ice.urls.size() - 1) std::cout << ", ";
        }
        if (!ice.username.empty()) {
            std::cout << " (authenticated)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    
    return true;
}

bool WebRTCClient::start() {
    if (is_streaming_) {
        std::cerr << "Already streaming" << std::endl;
        return false;
    }
    
    should_stop_ = false;
    is_streaming_ = true;
    frame_count_ = 0;
    
    // Start streaming thread
    streaming_thread_ = std::thread(&WebRTCClient::streamingThread, this);
    
    // Start signaling thread (for WebRTC handshake)
    signaling_thread_ = std::thread(&WebRTCClient::signalingThread, this);
    
    std::cout << "Streaming started" << std::endl;
    return true;
}

void WebRTCClient::stop() {
    if (!is_streaming_) {
        return;
    }
    
    should_stop_ = true;
    is_streaming_ = false;
    
    // Wake up threads
    queue_cv_.notify_all();
    
    // Wait for threads to finish
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }
    
    if (signaling_thread_.joinable()) {
        signaling_thread_.join();
    }
    
    // Clear frame queue
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }
    
    std::cout << "Streaming stopped. Total frames: " << frame_count_ << std::endl;
}

void WebRTCClient::streamingThread() {
    auto frame_duration = std::chrono::milliseconds(1000 / video_source_->getFrameRate());
    auto next_frame_time = std::chrono::steady_clock::now();
    
    while (!should_stop_) {
        cv::Mat frame;
        
        // Get frame from video source
        if (!video_source_->getFrame(frame)) {
            std::cerr << "Failed to get frame from video source" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Add timestamp overlay
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        cv::putText(frame, oss.str(), cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        
        // Add to queue for encoding/transmission
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (frame_queue_.size() >= MAX_QUEUE_SIZE) {
                frame_queue_.pop(); // Drop oldest frame if queue is full
            }
            frame_queue_.push(frame.clone());
        }
        queue_cv_.notify_one();
        
        frame_count_++;
        if (frame_count_ % 30 == 0) {
            std::cout << "Sent frame " << frame_count_ << std::endl;
        }
        
        // Maintain frame rate
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }
}

void WebRTCClient::signalingThread() {
    /*
     * Note: This is a simplified implementation.
     * For production use, you should implement proper WebRTC signaling
     * using the official WebRTC native API and a signaling server.
     * 
     * Steps for real implementation:
     * 1. Create PeerConnection
     * 2. Add video track from our source
     * 3. Create offer
     * 4. Exchange SDP via signaling server
     * 5. Handle ICE candidates
     * 6. Establish connection
     */
    
    std::cout << "Signaling thread started (simplified implementation)" << std::endl;
    std::cout << "Note: This demo uses a simplified WebRTC stub." << std::endl;
    std::cout << "For production, integrate with libwebrtc native API." << std::endl;
    
    // Simulate connection
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!should_stop_) {
        std::cout << "WebRTC connection established (simulated)" << std::endl;
    }
    
    // Process frame queue (in real implementation, this would encode and send via WebRTC)
    while (!should_stop_) {
        cv::Mat frame;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                             [this] { return !frame_queue_.empty() || should_stop_; });
            
            if (should_stop_) break;
            
            if (!frame_queue_.empty()) {
                frame = frame_queue_.front();
                frame_queue_.pop();
            }
        }
        
        if (!frame.empty()) {
            // In real implementation: encode frame and send via WebRTC data channel
            // For now, just simulate processing
            // std::cout << "Processing frame for transmission" << std::endl;
        }
    }
    
    std::cout << "Signaling thread stopped" << std::endl;
}
