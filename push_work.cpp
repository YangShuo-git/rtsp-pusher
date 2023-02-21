#include <functional>
#include "log.h"
#include "push_work.h"



PushWork::PushWork()
{

}

RET_CODE PushWork::Init(const Properties &properties)
{
    // 音频test模式
    audio_test_ = properties.GetProperty("audio_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");

    // 麦克风采样属性
    mic_sample_rate_ = properties.GetProperty("mic_sample_rate", 48000);
    mic_sample_fmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    mic_channels_ = properties.GetProperty("mic_channels", 2);

    // 设置音频捕获
    audio_capturer_ = new AudioCapturer();
    Properties aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test", 1);
    aud_cap_properties.SetProperty("input_pcm_name", input_pcm_name_);
    aud_cap_properties.SetProperty("channels", mic_channels_);
    //    aud_cap_properties.SetProperty("nb_samples", 1024);     // 由编码器提供
    if(audio_capturer_->Init(aud_cap_properties) != RET_OK)
    {
        LogError("AudioCapturer Init failed");
        return RET_FAIL;
    }

    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this, std::placeholders::_1,
                                           std::placeholders::_2));

    if(audio_capturer_->Start()!= RET_OK) {
         LogError("AudioCapturer Start failed");
        return RET_FAIL;
    }
    return RET_OK;
}

RET_CODE PushWork::DeInit()
{
    if(audio_capturer_) {
        audio_capturer_->Stop();
        delete audio_capturer_;
        audio_capturer_ = NULL;
    }
}
void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
    LogInfo("size:%d", size);
}
