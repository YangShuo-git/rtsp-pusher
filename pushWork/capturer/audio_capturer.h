#ifndef _AUDIO_CAPTURER_H_
#define _AUDIO_CAPTURER_H_

#include <functional>
#include "../common_looper.h"
#include "../media_base.h"
using std::function;


class AudioCapturer : public CommonLooper
{
public:
    AudioCapturer();
    virtual ~AudioCapturer();
    virtual void loop();

    /**
     * 
     * "sample_rate"     采样率，默认48000
     * "channels"        通道数量 ，默认2
     * "channel_layout"  通道布局，默认根据channels获取默认的
     */
    RET_CODE Init(const Properties properties);

    // 通过回调函数，将采集到的数据发送出去
    void AddCallback(function<void(uint8_t*, int32_t)> callback);

private:
    // PCM file只是用来测试, 写死为采样率48Khz 2通道 s16格式
    // 1帧1024采样点持续的时间21.33ms
    int openPcmFile(const char *file_name);
    int readPcmFile(uint8_t *pcm_buf, int32_t pcm_buf_size);
    int closePcmFile();

    int audio_test_ = 0;
    std::string input_pcm_name_;    // 输入pcm测试文件的名字
    FILE *pcm_fp_ = nullptr;

    int64_t pcm_start_time_ = 0;
    double pcm_total_duration_ = 0; // 推流时长的统计
    double frame_duration_ = 21.3;  // 默认帧长21.3ms 根据采样率和音频帧大小计算的

    uint8_t *pcm_buf_;     // 每次都是从pcm文件中读取一个音频帧  
    int32_t pcm_buf_size_; // 从pcm文件中读取的一个音频帧的大小

    int sample_rate_ = 48000; // 采样率
    int format_ = 1;          // 采样格式 目前固定s16
    int channels_ = 2;        // 声道数
    int nb_samples_ = 1024;   // 一个音频帧的采样点
    int byte_per_sample_ = 2; // 一个采样点的大小，单位字节

    bool is_first_time_ = false;

    std::function<void(uint8_t *, int32_t)> callback_handle_pcm_;
};

#endif // _AUDIO_CAPTURER_H_