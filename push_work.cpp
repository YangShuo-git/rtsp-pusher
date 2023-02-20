#ifndef _PUSH_WORK_H_
#define _PUSH_WORK_H_

#include <string>
#include "audio_capturer.h"

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
private:
    AudioCapturer *audio_capturer_ = NULL;
    // 音频test模式
    int audio_test_ = 0;
    std::string input_pcm_name_;

    // 麦克风采样属性
    int mic_sample_rate_ = 48000;
    int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;
    int mic_channels_ = 2;
};

#endif // _PUSH_WORK_H_