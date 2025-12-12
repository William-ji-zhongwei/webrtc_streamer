#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include "video_source.h"
#ifdef ENABLE_REALSENSE
#include "realsense_source.h"
#endif
#include "opencv_source.h"
#include "webrtc_client.h"
#include "config_parser.h"

std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --config <file>       配置文件路径 (default: config/config.json)" << std::endl;
    std::cout << "  --create-config       创建默认配置文件并退出" << std::endl;
    std::cout << "  --source <type>       视频源类型: realsense|camera|file|rtsp" << std::endl;
    std::cout << "  --device <id>         相机设备 ID (for camera source)" << std::endl;
    std::cout << "  --file <path>         视频文件路径或 RTSP URL" << std::endl;
    std::cout << "  --width <width>       视频宽度" << std::endl;
    std::cout << "  --height <height>     视频高度" << std::endl;
    std::cout << "  --fps <fps>           帧率" << std::endl;
    std::cout << "  --depth               启用深度流 (RealSense)" << std::endl;
    std::cout << "  --server <ip>         服务器 IP 地址" << std::endl;
    std::cout << "  --port <port>         服务器端口" << std::endl;
    std::cout << "  --help                显示帮助信息" << std::endl;
    std::cout << "\n说明:" << std::endl;
    std::cout << "  - 命令行参数会覆盖配置文件中的设置" << std::endl;
    std::cout << "  - STUN/TURN 服务器配置请编辑 config/config.json 文件" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " --config my_config.json" << std::endl;
    std::cout << "  " << program_name << " --create-config" << std::endl;
    std::cout << "  " << program_name << " --source camera --device 0" << std::endl;
    std::cout << "  " << program_name << " --source rtsp --file rtsp://example.com/stream" << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Configuration
    ConfigParser config_parser;
    std::string config_file = "config/config.json";
    bool use_config_file = true;
    
    // Parse command line arguments (first pass - check for config file and create-config)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--create-config") {
            std::string output_file = "config/config.json";
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                output_file = argv[++i];
            }
            if (ConfigParser::createDefaultConfig(output_file)) {
                std::cout << "配置文件已创建，请编辑后使用" << std::endl;
                return 0;
            } else {
                return 1;
            }
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        }
    }
    
    // Load configuration from file
    config_parser.loadFromFile(config_file);
    AppConfig& config = config_parser.getConfig();
    
    // Parse command line arguments (second pass - override config)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--config") {
            i++; // Skip, already processed
        } else if (arg == "--source" && i + 1 < argc) {
            config.video.source = argv[++i];
        } else if (arg == "--device" && i + 1 < argc) {
            config.video.device_id = std::stoi(argv[++i]);
        } else if (arg == "--file" && i + 1 < argc) {
            config.video.file_path = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            config.video.width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.video.height = std::stoi(argv[++i]);
        } else if (arg == "--fps" && i + 1 < argc) {
            config.video.fps = std::stoi(argv[++i]);
        } else if (arg == "--depth") {
            config.video.enable_depth = true;
        } else if (arg == "--server" && i + 1 < argc) {
            config.webrtc.server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.webrtc.server_port = std::stoi(argv[++i]);
        } else if (arg != "--help" && arg != "--create-config") {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Print current configuration
    config_parser.printConfig();
    
    // Extract config values for easier access
    std::string source_type = config.video.source;
    int device_id = config.video.device_id;
    std::string file_path = config.video.file_path;
    int width = config.video.width;
    int height = config.video.height;
    int fps = config.video.fps;
    bool enable_depth = config.video.enable_depth;
    std::string server_ip = config.webrtc.server_ip;
    int server_port = config.webrtc.server_port;

    // Create video source based on type
    std::shared_ptr<VideoSource> video_source;
    
    if (source_type == "realsense") {
#ifdef ENABLE_REALSENSE
        std::cout << "Using Intel RealSense camera" << std::endl;
        video_source = std::make_shared<RealSenseSource>(width, height, fps, enable_depth);
#else
        std::cerr << "Error: RealSense support not compiled. Rebuild with -DENABLE_REALSENSE=ON" << std::endl;
        return 1;
#endif
    } else if (source_type == "camera") {
        std::cout << "Using USB/OpenCV camera" << std::endl;
        video_source = std::make_shared<OpenCVSource>(device_id, width, height, fps);
    } else if (source_type == "file" || source_type == "rtsp") {
        if (file_path.empty()) {
            std::cerr << "Error: --file parameter required for file/rtsp source" << std::endl;
            return 1;
        }
        std::cout << "Using video file/stream: " << file_path << std::endl;
        video_source = std::make_shared<OpenCVSource>(file_path, fps);
    } else {
        std::cerr << "Unknown source type: " << source_type << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Initialize video source
    if (!video_source->initialize()) {
        std::cerr << "Failed to initialize video source" << std::endl;
        return 1;
    }

    // Create WebRTC client
    auto webrtc_client = std::make_unique<WebRTCClient>(video_source, config.webrtc);
    
    if (!webrtc_client->initialize()) {
        std::cerr << "Failed to initialize WebRTC client" << std::endl;
        video_source->release();
        return 1;
    }

    // Start streaming
    if (!webrtc_client->start()) {
        std::cerr << "Failed to start streaming" << std::endl;
        video_source->release();
        return 1;
    }

    std::cout << "\n=== Streaming Started ===" << std::endl;
    std::cout << "Press Ctrl+C to stop...\n" << std::endl;

    // Main loop
    while (g_running && webrtc_client->isStreaming()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    std::cout << "\nCleaning up..." << std::endl;
    webrtc_client->stop();
    video_source->release();

    std::cout << "Shutdown complete." << std::endl;
    return 0;
}
