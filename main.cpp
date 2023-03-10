#include <iostream>
#include "log.h"
#include "push_work.h"
#include "msg_queue.h"
using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

// 减少缓冲 ffplay.exe -i rtmp://xxxxxxx -fflags nobuffer
// 减少码流分析时间 ffplay.exe -i rtmp://xxxxxxx -analyzeduration 1000000 单位为微秒
// ffplay -i rtsp://192.168.2.132/live/livestream -fflags nobuffer -analyzeduration 1000000 -rtsp_transport udp

#define RTSP_URL "rtsp://192.168.183.191/live/livestream"  // ubuntu IP 桥接模式
// ffmpeg -re -i  rtsp_test_hd.flv  -vcodec copy -acodec copy  -f flv -y rtsp://111.229.231.225/live/livestream
// ffmpeg -re -i  rtsp_test_hd.flv  -vcodec copy -acodec copy  -f flv -y rtsp://192.168.1.12/live/livestream
// ffmpeg -re -i  1920x832_25fps.flv  -vcodec copy -acodec copy  -f flv -y rtsp://111.229.231.225/live/livestream


int main()
{
    init_logger("rtsp_push.log", S_INFO);
    MessageQueue *msg_queue = new MessageQueue();

    // for (int i = 0; i < 3; i++)
    {   
        // printf("this is %d time!\n", i);
        // 测试生命周期
        if(!msg_queue) {
            LogError("new MessageQueue() failed");
            return -1;
        }
        PushWork push_work(msg_queue);
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

        // 配置rtsp
        properties.SetProperty("rtsp_url", RTSP_URL);
        properties.SetProperty("rtsp_transport", "udp");
        properties.SetProperty("rtsp_timeout", 3000);  // 超时时间 3s
        properties.SetProperty("rtsp_max_queue_duration", 1000);

        // 启动push_work
        if(push_work.Init(properties) != RET_OK) {
            LogError("Fail to init PushWork");
            return -1;
        }

        // 设置采集的时间
        int ret = 0;
        int count = 0;
        AVMessage msg;
        while (true)  // 这里阻塞的时间，就是采集的时间
        { 
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            // ret = msg_queue->msg_queue_get(&msg, 1000);
            // if(1 == ret) {
            //     switch (msg.what) {
            //     case MSG_RTSP_ERROR:
            //         LogError("MSG_RTSP_ERROR error:%d", msg.arg1);
            //         break;
            //     case MSG_RTSP_QUEUE_DURATION:
            //         LogError("MSG_RTSP_QUEUE_DURATION a:%d, v:%d", msg.arg1, msg.arg2);
            //         break;
            //     default:
            //         break;
            //     }
            // }
            if(count++ > 1000){
                LogInfo("Main break\n");
                break;
            }
        }
        delete msg_queue;
    }

    LogInfo("Main finish\n");
    return 0;
}
