#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <vector>
#include <memory>

/**
 * @brief ICE Server configuration
 */
struct IceServer {
    std::vector<std::string> urls;
    std::string username;
    std::string credential;
    
    IceServer() = default;
    explicit IceServer(const std::vector<std::string>& urls_) : urls(urls_) {}
    IceServer(const std::vector<std::string>& urls_, 
              const std::string& user, 
              const std::string& cred)
        : urls(urls_), username(user), credential(cred) {}
};

/**
 * @brief WebRTC configuration
 */
struct WebRTCConfig {
    std::string server_ip;
    int server_port;
    std::string client_id;      // 客户端 ID
    std::string target_id;      // 目标接收方 ID（可选，为空则广播）
    std::vector<IceServer> ice_servers;
    
    WebRTCConfig() : server_ip("192.168.1.34"), server_port(50061),
                     client_id("sender_001"), target_id("") {
        // 默认添加 Google STUN 服务器
        std::vector<std::string> stun_urls = {"stun:stun.l.google.com:19302"};
        ice_servers.push_back(IceServer(stun_urls));
    }
};

/**
 * @brief Video source configuration
 */
struct VideoConfig {
    std::string source;
    int width;
    int height;
    int fps;
    int device_id;
    std::string file_path;
    bool enable_depth;
    
    VideoConfig() : source("realsense"), width(640), height(480), 
                    fps(30), device_id(0), enable_depth(false) {}
};

/**
 * @brief Logging configuration
 */
struct LogConfig {
    std::string level;
    bool enable_timestamp;
    
    LogConfig() : level("info"), enable_timestamp(true) {}
};

/**
 * @brief Application configuration
 */
struct AppConfig {
    WebRTCConfig webrtc;
    VideoConfig video;
    LogConfig logging;
};

/**
 * @brief Configuration parser class
 */
class ConfigParser {
public:
    ConfigParser() = default;
    
    /**
     * @brief Load configuration from JSON file
     * @param config_file Path to config file
     * @return true if successful
     */
    bool loadFromFile(const std::string& config_file);
    
    /**
     * @brief Get the configuration
     * @return Reference to AppConfig
     */
    const AppConfig& getConfig() const { return config_; }
    
    /**
     * @brief Get mutable configuration (for command line override)
     * @return Reference to AppConfig
     */
    AppConfig& getConfig() { return config_; }
    
    /**
     * @brief Print current configuration
     */
    void printConfig() const;
    
    /**
     * @brief Create default config file
     * @param config_file Path to config file
     * @return true if successful
     */
    static bool createDefaultConfig(const std::string& config_file);

private:
    AppConfig config_;
};

#endif // CONFIG_PARSER_H
