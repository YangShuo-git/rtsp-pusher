#ifndef _VIDEO_CAPTURER_H_
#define _VIDEO_CAPTURER_H_

#include <functional>
#include "../common_looper.h"
#include "../media_base.h"
using std::function;

class VideoCapturer: public CommonLooper
{
public:
    VideoCapturer();
    ~VideoCapturer();
    virtual void loop();

    /**
     * "x", x起始位置，默认为0
     * "y", y起始位置，默认为0
     * "width", 宽度，默认为屏幕宽带
     * "height", 高度，默认为屏幕高度
     * "format", 像素格式，AVPixelFormat对应的值，默认为AV_PIX_FMT_YUV420P
     * "fps", 帧数，默认为25
     */
    RET_CODE Init(const Properties& properties);
    void AddCallback(function<void(uint8_t*, int32_t)> callback);

private:
    int video_test_ = 0;
    std::string input_yuv_name_;
    int x_;  // x起始位置，默认为0
    int y_;  // y起始位置，默认为0
    int width_ = 0;  // 宽度，默认为屏幕宽度
    int height_ = 0; // 高度，默认为屏幕高度
    int pixel_format_ = 0; // 像素格式，AVPixelFormat对应的值，默认为AV_PIX_FMT_YUV420P
    int fps_;  // 帧率，默认为25
    double frame_duration_ = 40; // 帧长 默认为40ms

    // 本地文件测试
    int openYuvFile(const char *file_name);
    int readYuvFile(uint8_t *yuv_buf, int32_t yuv_buf_size);
    int closeYuvFile();

    int64_t yuv_start_time_ = 0;     // 起始时间
    double yuv_total_duration_ = 0;  // YUV读取累计的时间
    FILE *yuv_fp_ = nullptr;
    uint8_t *yuv_buf_ = nullptr;
    int yuv_buf_size = 0;


    function<void(uint8_t*, int32_t)> callback_handle_yuv_ = nullptr;
    bool is_first_frame_ = false;
};

#endif // _VIDEO_CAPTURER_H_