#include "config_parser.h"
#include <fstream>
#include <iostream>
#include <sstream>

// 简单的 JSON 解析（生产环境建议使用 nlohmann/json 或其他成熟库）
// 这里实现一个简化版本以避免额外依赖

namespace {
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\"");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\",");
        return str.substr(first, (last - first + 1));
    }
    
    std::string getJsonValue(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";
        
        pos++;
        size_t end = json.find_first_of(",}", pos);
        if (end == std::string::npos) return "";
        
        return trim(json.substr(pos, end - pos));
    }
    
    std::vector<std::string> parseJsonArray(const std::string& json, const std::string& key) {
        std::vector<std::string> result;
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return result;
        
        pos = json.find("[", pos);
        if (pos == std::string::npos) return result;
        
        size_t end = json.find("]", pos);
        if (end == std::string::npos) return result;
        
        std::string array_content = json.substr(pos + 1, end - pos - 1);
        std::istringstream iss(array_content);
        std::string item;
        
        while (std::getline(iss, item, ',')) {
            std::string trimmed = trim(item);
            if (!trimmed.empty()) {
                result.push_back(trimmed);
            }
        }
        
        return result;
    }
}

bool ConfigParser::loadFromFile(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << config_file << std::endl;
        std::cerr << "使用默认配置" << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    try {
        // 解析 WebRTC 配置
        std::string server_ip = getJsonValue(json, "ip");
        if (!server_ip.empty()) {
            config_.webrtc.server_ip = server_ip;
        }
        
        std::string port_str = getJsonValue(json, "port");
        if (!port_str.empty()) {
            config_.webrtc.server_port = std::stoi(port_str);
        }
        
        // 解析 ICE servers (简化版本 - 只解析第一个 STUN 和 TURN)
        // 生产环境应使用完整的 JSON 库
        size_t ice_pos = json.find("\"ice_servers\"");
        if (ice_pos != std::string::npos) {
            config_.webrtc.ice_servers.clear();
            
            // 查找所有 urls 数组
            size_t search_pos = ice_pos;
            while (true) {
                size_t urls_pos = json.find("\"urls\"", search_pos);
                if (urls_pos == std::string::npos || urls_pos > json.find("]", ice_pos)) break;
                
                size_t arr_start = json.find("[", urls_pos);
                size_t arr_end = json.find("]", arr_start);
                std::string urls_content = json.substr(arr_start + 1, arr_end - arr_start - 1);
                
                std::vector<std::string> urls;
                std::istringstream iss(urls_content);
                std::string url;
                while (std::getline(iss, url, ',')) {
                    std::string trimmed = trim(url);
                    if (!trimmed.empty()) {
                        urls.push_back(trimmed);
                    }
                }
                
                // 查找对应的 username 和 credential
                size_t obj_start = json.rfind("{", urls_pos);
                size_t obj_end = json.find("}", urls_pos);
                std::string obj = json.substr(obj_start, obj_end - obj_start);
                
                std::string username = getJsonValue(obj, "username");
                std::string credential = getJsonValue(obj, "credential");
                
                if (!username.empty() && !credential.empty()) {
                    config_.webrtc.ice_servers.push_back(IceServer(urls, username, credential));
                } else {
                    config_.webrtc.ice_servers.push_back(IceServer(urls));
                }
                
                search_pos = obj_end;
            }
        }
        
        // 解析 Video 配置
        std::string source = getJsonValue(json, "source");
        if (!source.empty()) {
            config_.video.source = source;
        }
        
        std::string width = getJsonValue(json, "width");
        if (!width.empty()) {
            config_.video.width = std::stoi(width);
        }
        
        std::string height = getJsonValue(json, "height");
        if (!height.empty()) {
            config_.video.height = std::stoi(height);
        }
        
        std::string fps = getJsonValue(json, "fps");
        if (!fps.empty()) {
            config_.video.fps = std::stoi(fps);
        }
        
        std::string device_id = getJsonValue(json, "device_id");
        if (!device_id.empty()) {
            config_.video.device_id = std::stoi(device_id);
        }
        
        std::string file_path = getJsonValue(json, "file_path");
        if (!file_path.empty()) {
            config_.video.file_path = file_path;
        }
        
        std::string enable_depth = getJsonValue(json, "enable_depth");
        if (!enable_depth.empty()) {
            config_.video.enable_depth = (enable_depth == "true");
        }
        
        // 解析 Logging 配置
        std::string level = getJsonValue(json, "level");
        if (!level.empty()) {
            config_.logging.level = level;
        }
        
        std::string enable_timestamp = getJsonValue(json, "enable_timestamp");
        if (!enable_timestamp.empty()) {
            config_.logging.enable_timestamp = (enable_timestamp == "true");
        }
        
        std::cout << "配置文件加载成功: " << config_file << std::endl;
        return true;
        
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
