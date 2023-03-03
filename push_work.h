#ifndef _PUSH_WORK_H_
#define _PUSH_WORK_H_

#include <string>
#include "audio_capturer.h"
#include "video_capturer.h"
#include "aac_encoder.h"

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
    ~PushWork();
    RET_CODE Init(const Properties &properties);
    RET_CODE DeInit();
private:
    void PcmCallback(uint8_t *pcm, int32_t size);
    void YuvCallback(uint8_t* yuv, int32_t size);

private:
    AudioCapturer *audio_capturer_ = nullptr;
    VideoCapturer *video_capturer_ = nullptr;

    AACEncoder *audio_encoder_;
    AVFrame *audio_frame_ = nullptr;
    FILE *pcm_s16le_fp_ = nullptr;
    FILE *aac_fp_ = nullptr;
    uint8_t *fltp_buf_ = nullptr;
    int fltp_buf_size_ = 0;

    // 文件模式
    int audio_test_ = 0;
    std::string input_pcm_name_;
    int video_test_ = 0;
    std::string input_yuv_name_;

    // 麦克风采样属性
    int mic_sample_rate_ = 48000;  // 采样率
    int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;  // 采样格式
    int mic_channels_ = 2;  // 通道数

    // 桌面录制属性
    int desktop_x_ = 0;
    int desktop_y_ = 0;
    int desktop_width_  = 1920;
    int desktop_height_ = 1080;
    int desktop_format_ = AV_PIX_FMT_YUV420P;
    int desktop_fps_ = 25;

    // 音频编码参数
    int audio_sample_rate_ = 48000;
    int audio_sample_fmt_ ;  // 具体由编码器决定，从编码器读取相应的信息
    int audio_channels_ = 2;
    int audio_bitrate_ = 128*1024;
    int audio_ch_layout_;  // 由audio_channels_决定
};

#endif // _PUSH_WORK_H_