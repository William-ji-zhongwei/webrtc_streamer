#ifndef SIMPLE_VIDEO_CODEC_FACTORY_H
#define SIMPLE_VIDEO_CODEC_FACTORY_H

#include <memory>
#include <vector>
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

namespace webrtc {

// 视频编码器工厂（支持 VP8 和 H.264）
class SimpleVideoEncoderFactory : public VideoEncoderFactory {
public:
    SimpleVideoEncoderFactory() {
        std::cout << "SimpleVideoEncoderFactory created" << std::endl;
    }

    std::vector<SdpVideoFormat> GetSupportedFormats() const override {
        std::cout << "SimpleVideoEncoderFactory::GetSupportedFormats called" << std::endl;
        std::vector<SdpVideoFormat> formats;
        // VP8
        formats.push_back(SdpVideoFormat("VP8"));
        
        // H.264 Constrained Baseline Profile, Level 3.1
        SdpVideoFormat h264("H264");
        h264.parameters["level-asymmetry-allowed"] = "1";
        h264.parameters["packetization-mode"] = "1";
        h264.parameters["profile-level-id"] = "42e01f";
        formats.push_back(h264);

        // Generic H264 (no params)
        formats.push_back(SdpVideoFormat("H264"));
        
        return formats;
    }

    CodecSupport QueryCodecSupport(
        const SdpVideoFormat& format,
        absl::optional<std::string> scalability_mode) const override {
        std::cout << "SimpleVideoEncoderFactory::QueryCodecSupport called for " << format.name << std::endl;
        return VideoEncoderFactory::QueryCodecSupport(format, scalability_mode);
    }

    std::unique_ptr<VideoEncoder> CreateVideoEncoder(
        const SdpVideoFormat& format) override {
        std::cout << "Creating video encoder for format: " << format.name << std::endl;
        if (format.name == "VP8") {
            return VP8Encoder::Create();
        } else if (format.name == "H264") {
            return H264Encoder::Create();
        }
        return nullptr;
    }
};

// 视频解码器工厂
class SimpleVideoDecoderFactory : public VideoDecoderFactory {
public:
    std::vector<SdpVideoFormat> GetSupportedFormats() const override {
        std::vector<SdpVideoFormat> formats;
        formats.push_back(SdpVideoFormat("VP8"));
        formats.push_back(SdpVideoFormat("H264"));
        return formats;
    }

    std::unique_ptr<VideoDecoder> CreateVideoDecoder(
        const SdpVideoFormat& format) override {
        if (format.name == "VP8") {
            return VP8Decoder::Create();
        } else if (format.name == "H264") {
            return H264Decoder::Create();
        }
        return nullptr;
    }
};

}  // namespace webrtc

#endif  // SIMPLE_VIDEO_CODEC_FACTORY_H
