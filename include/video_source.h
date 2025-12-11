#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <memory>
#include <string>
#include <opencv2/opencv.hpp>

/**
 * @brief Abstract base class for video sources
 * 
 * This class provides an interface for different video sources
 * (RealSense camera, USB camera, RTSP stream, etc.)
 */
class VideoSource {
public:
    virtual ~VideoSource() = default;

    /**
     * @brief Initialize the video source
     * @return true if successful, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Get the next frame from the video source
     * @param frame Output frame in BGR format
     * @return true if frame was successfully retrieved, false otherwise
     */
    virtual bool getFrame(cv::Mat& frame) = 0;

    /**
     * @brief Get the width of the video frames
     * @return Width in pixels
     */
    virtual int getWidth() const = 0;

    /**
     * @brief Get the height of the video frames
     * @return Height in pixels
     */
    virtual int getHeight() const = 0;

    /**
     * @brief Get the frame rate
     * @return Frames per second
     */
    virtual int getFrameRate() const = 0;

    /**
     * @brief Release resources
     */
    virtual void release() = 0;

    /**
     * @brief Get the name/description of the video source
     * @return Source name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Check if the video source is open and ready
     * @return true if ready, false otherwise
     */
    virtual bool isReady() const = 0;
};

#endif // VIDEO_SOURCE_H
