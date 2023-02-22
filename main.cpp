#include <iostream>
#include "log.h"
#include "push_work.h"
using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

int main()
{
    init_logger("rtsp_push.log", S_INFO);

    {
        PushWork push_work;
        Properties properties;

        // 音频test模式
        properties.SetProperty("audio_test", 1);    // 音频测试模式
        properties.SetProperty("input_pcm_name", "./res/buweishui_48000_2_s16le.pcm");
        // 麦克风采样属性
        properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
        properties.SetProperty("mic_sample_rate", 48000);
        properties.SetProperty("mic_channels", 2);

        if(push_work.Init(properties) != RET_OK) {
            LogError("PushWork init failed");
            return -1;
        }
        int count = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if(count++ > 5)
                break;
        }

    }
    LogInfo("main finish");
    return 0;
}
