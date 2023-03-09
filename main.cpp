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

    {   // 测试生命周期
        PushWork push_work;
        Properties properties;

        // 音频test模式
        properties.SetProperty("audio_test", 1);
        properties.SetProperty("input_pcm_name", "./res/48000_2_s16le.pcm");

        // 视频test模式
        properties.SetProperty("video_test", 1);
        properties.SetProperty("input_yuv_name", "./res/720x480_25fps_420p.yuv");

#if 0
        // 麦克风采样属性
        properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
        properties.SetProperty("mic_sample_rate", 48000);
        properties.SetProperty("mic_channels", 2);

        // 桌面录制属性
        properties.SetProperty("desktop_x", 0);
        properties.SetProperty("desktop_y", 0);
        properties.SetProperty("desktop_width", 720);   //测试模式时和yuv文件的宽度一致
        properties.SetProperty("desktop_height", 480);  //测试模式时和yuv文件的高度一致
        //    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
        properties.SetProperty("desktop_fps", 25);//测试模式时和yuv文件的帧率一致
        // 视频编码属性
        properties.SetProperty("video_bitrate", 512*1024);  // 设置码率

#endif

        // 音频编码属性
        properties.SetProperty("audio_sample_rate", 48000);
        properties.SetProperty("audio_channels", 2);
        properties.SetProperty("audio_bitrate", 64*1024);

        // 视频编码属性
        properties.SetProperty("video_bitrate", 512*1024);  // 设置码率

        // 启动push_work
        if(push_work.Init(properties) != RET_OK) {
            LogError("Fail to init PushWork");
            return -1;
        }
        int count = 0;
        while (true)  // 这里阻塞的时间，就是采集的时间
        { 
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if(count++ > 5)
                break;
        }
    }
    LogInfo("main finish");
    return 0;
}
