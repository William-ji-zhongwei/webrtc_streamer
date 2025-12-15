#include "config_parser.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool ConfigParser::loadFromFile(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << config_file << std::endl;
        std::cerr << "使用默认配置" << std::endl;
        return false;
    }
    
    try {
        json j = json::parse(file);
        
        // 解析 WebRTC 配置
        if (j.contains("webrtc")) {
            auto& webrtc = j["webrtc"];
            
            // 解析服务器配置
            if (webrtc.contains("server")) {
                auto& server = webrtc["server"];
                if (server.contains("ip")) {
                    config_.webrtc.server_ip = server["ip"].get<std::string>();
                }
                if (server.contains("port")) {
                    config_.webrtc.server_port = server["port"].get<int>();
                }
            }
            
            // 解析客户端 ID 和目标 ID
            if (webrtc.contains("client_id")) {
                config_.webrtc.client_id = webrtc["client_id"].get<std::string>();
            }
            if (webrtc.contains("target_id")) {
                config_.webrtc.target_id = webrtc["target_id"].get<std::string>();
            }
            
            // 解析 ICE servers
            if (webrtc.contains("ice_servers")) {
                config_.webrtc.ice_servers.clear();
                
                for (auto& ice_server : webrtc["ice_servers"]) {
                    std::vector<std::string> urls;
                    
                    // 解析 URLs
                    if (ice_server.contains("urls")) {
                        if (ice_server["urls"].is_array()) {
                            urls = ice_server["urls"].get<std::vector<std::string>>();
                        } else {
                            urls.push_back(ice_server["urls"].get<std::string>());
                        }
                    }
                    
                    // 解析认证信息
                    if (ice_server.contains("username") && ice_server.contains("credential")) {
                        std::string username = ice_server["username"].get<std::string>();
                        std::string credential = ice_server["credential"].get<std::string>();
                        config_.webrtc.ice_servers.push_back(IceServer(urls, username, credential));
                    } else {
                        config_.webrtc.ice_servers.push_back(IceServer(urls));
                    }
                }
            }
        }
        
        // 解析 Video 配置
        if (j.contains("video")) {
            auto& video = j["video"];
            
            if (video.contains("source")) {
                config_.video.source = video["source"].get<std::string>();
            }
            if (video.contains("width")) {
                config_.video.width = video["width"].get<int>();
            }
            if (video.contains("height")) {
                config_.video.height = video["height"].get<int>();
            }
            if (video.contains("fps")) {
                config_.video.fps = video["fps"].get<int>();
            }
            if (video.contains("device_id")) {
                config_.video.device_id = video["device_id"].get<int>();
            }
            if (video.contains("file_path")) {
                config_.video.file_path = video["file_path"].get<std::string>();
            }
            if (video.contains("enable_depth")) {
                config_.video.enable_depth = video["enable_depth"].get<bool>();
            }
        }
        
        // 解析 Logging 配置
        if (j.contains("logging")) {
            auto& logging = j["logging"];
            
            if (logging.contains("level")) {
                config_.logging.level = logging["level"].get<std::string>();
            }
            if (logging.contains("enable_timestamp")) {
                config_.logging.enable_timestamp = logging["enable_timestamp"].get<bool>();
            }
        }
        
        std::cout << "配置文件加载成功: " << config_file << std::endl;
        return true;
        
    } catch (const json::parse_error& e) {
        std::cerr << "JSON 解析错误: " << e.what() << std::endl;
        std::cerr << "使用默认配置" << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "解析配置文件出错: " << e.what() << std::endl;
        std::cerr << "使用默认配置" << std::endl;
        return false;
    }
}

void ConfigParser::printConfig() const {
    std::cout << "\n========================================" << std::endl;
    std::cout << "当前配置:" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n[WebRTC]" << std::endl;
    std::cout << "  服务器: " << config_.webrtc.server_ip 
              << ":" << config_.webrtc.server_port << std::endl;
    std::cout << "  ICE 服务器 (" << config_.webrtc.ice_servers.size() << "):" << std::endl;
    for (size_t i = 0; i < config_.webrtc.ice_servers.size(); i++) {
        const auto& ice = config_.webrtc.ice_servers[i];
        std::cout << "    [" << i + 1 << "] URLs: ";
        for (size_t j = 0; j < ice.urls.size(); j++) {
            std::cout << ice.urls[j];
            if (j < ice.urls.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        if (!ice.username.empty()) {
            std::cout << "        Username: " << ice.username << std::endl;
            std::cout << "        Credential: " << std::string(ice.credential.length(), '*') << std::endl;
        }
    }
    
    std::cout << "\n[Video]" << std::endl;
    std::cout << "  源类型: " << config_.video.source << std::endl;
    std::cout << "  分辨率: " << config_.video.width << "x" << config_.video.height << std::endl;
    std::cout << "  帧率: " << config_.video.fps << " fps" << std::endl;
    if (config_.video.source == "camera") {
        std::cout << "  设备ID: " << config_.video.device_id << std::endl;
    }
    if (!config_.video.file_path.empty()) {
        std::cout << "  文件路径: " << config_.video.file_path << std::endl;
    }
    if (config_.video.source == "realsense") {
        std::cout << "  深度流: " << (config_.video.enable_depth ? "启用" : "禁用") << std::endl;
    }
    
    std::cout << "\n[Logging]" << std::endl;
    std::cout << "  级别: " << config_.logging.level << std::endl;
    std::cout << "  时间戳: " << (config_.logging.enable_timestamp ? "启用" : "禁用") << std::endl;
    
    std::cout << "========================================\n" << std::endl;
}

bool ConfigParser::createDefaultConfig(const std::string& config_file) {
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "无法创建配置文件: " << config_file << std::endl;
        return false;
    }
    
    file << R"({
  "webrtc": {
    "server": {
      "ip": "192.168.1.34",
      "port": 50061
    },
    "ice_servers": [
      {
        "urls": ["stun:stun.l.google.com:19302"]
      },
      {
        "urls": ["stun:stun1.l.google.com:19302"]
      },
      {
        "urls": ["turn:turn.example.com:3478"],
        "username": "your_username",
        "credential": "your_password"
      }
    ]
  },
  "video": {
    "source": "realsense",
    "width": 640,
    "height": 480,
    "fps": 30,
    "device_id": 0,
    "file_path": "",
    "enable_depth": false
  },
  "logging": {
    "level": "info",
    "enable_timestamp": true
  }
}
)";
    
    file.close();
    std::cout << "默认配置文件已创建: " << config_file << std::endl;
    return true;
}
