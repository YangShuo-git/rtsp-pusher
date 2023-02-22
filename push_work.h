#ifndef _PUSH_WORK_H_
#define _PUSH_WORK_H_

#include <string>
#include "audio_capturer.h"
#include "video_capturer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/audio_fifo.h"
}

class PushWork
{
public:
    PushWork();
    RET_CODE Init(const Properties &properties);
    RET_CODE DeInit();
private:
    void PcmCallback(uint8_t *pcm, int32_t size);
    void YuvCallback(uint8_t* yuv, int32_t size);

private:
    AudioCapturer *audio_capturer_ = NULL;
    // 音频test模式
    int audio_test_ = 0;
    std::string input_pcm_name_;

    // 麦克风采样属性
    int mic_sample_rate_ = 48000;
    int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;
    int mic_channels_ = 2;

    // 视频test模式
    int video_test_ = 0;
    std::string input_yuv_name_;

    // 桌面录制属性
    int desktop_x_ = 0;
    int desktop_y_ = 0;
    int desktop_width_  = 1920;
    int desktop_height_ = 1080;
    int desktop_format_ = AV_PIX_FMT_YUV420P;
    int desktop_fps_ = 25;

    // 视频相关
    VideoCapturer *video_capturer_ = nullptr;
};

#endif // _PUSH_WORK_H_