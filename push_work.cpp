#include <functional>
#include "log.h"
#include "push_work.h"
#include "avpublish_time.h"

PushWork::PushWork()
{

}

RET_CODE PushWork::Init(const Properties &properties)
{
    // 音频test模式
    audio_test_ = properties.GetProperty("audio_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");

    // 视频test模式
    video_test_ = properties.GetProperty("video_test", 0);
    input_yuv_name_ = properties.GetProperty("input_yuv_name", "input_1280_720_420p.yuv");

#if 0
    // 麦克风采样属性
    mic_sample_rate_ = properties.GetProperty("mic_sample_rate", 48000);
    mic_sample_fmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    mic_channels_ = properties.GetProperty("mic_channels", 2);
    // mic_nb_samples_ = properties.GetProperty("mic_nb_samples_", 1024);

    // 桌面录制属性
    desktop_x_ = properties.GetProperty("desktop_x", 0);
    desktop_y_ = properties.GetProperty("desktop_y", 0);
    desktop_width_  = properties.GetProperty("desktop_width", 1920);
    desktop_height_ = properties.GetProperty("desktop_height", 1080);
    desktop_format_ = properties.GetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
    desktop_fps_ = properties.GetProperty("desktop_fps", 25);
#endif

    // 设置音频编码器，先音频捕获初始化
    audio_encoder_ = new AACEncoder();
    if(!audio_encoder_)
    {
        LogError("Fail to new AACEncoder");
        return RET_FAIL;
    }
    Properties  aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate", audio_sample_rate_);
    aud_codec_properties.SetProperty("channels", audio_channels_);
    aud_codec_properties.SetProperty("bitrate", audio_bitrate_);        // 这里没有去设置采样格式
    // 需要什么样的采样格式是从编码器读取出来的
    if(audio_encoder_->Init(aud_codec_properties) != RET_OK)
    {
        LogError("Fail to AACEncoder Init");
        return RET_FAIL;
    }

    // 设置音频捕获
    audio_capturer_ = new AudioCapturer();
    Properties aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test", 1);
    aud_cap_properties.SetProperty("input_pcm_name", input_pcm_name_);
    aud_cap_properties.SetProperty("sample_rate", 48000);
    aud_cap_properties.SetProperty("format", AV_SAMPLE_FMT_S16);
    aud_cap_properties.SetProperty("channels", mic_channels_);
    aud_cap_properties.SetProperty("nb_samples", 1024);     // 由编码器提供
    aud_cap_properties.SetProperty("byte_per_sample", 2);    

    if(audio_capturer_->Init(aud_cap_properties) != RET_OK)
    {
        LogError("AudioCapturer Init failed");
        return RET_FAIL;
    }
    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this, 
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    if(audio_capturer_->Start()!= RET_OK) 
    {
         LogError("AudioCapturer Start failed");
        return RET_FAIL;
    }

    // 设置视频捕获
    video_capturer_ = new VideoCapturer();
    Properties vid_cap_properties;
    vid_cap_properties.SetProperty("video_test", 1);
    vid_cap_properties.SetProperty("input_yuv_name", input_yuv_name_);
    vid_cap_properties.SetProperty("width", desktop_width_);
    vid_cap_properties.SetProperty("height", desktop_height_);

    if(video_capturer_->Init(vid_cap_properties) != RET_OK)
    {
        LogError("VideoCapturer Init failed");
        return RET_FAIL;
    }
//    video_nalu_buf = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];
    video_capturer_->AddCallback(std::bind(&PushWork::YuvCallback, this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    if(video_capturer_->Start()!= RET_OK) {
        LogError("VideoCapturer Start failed");
        return RET_FAIL;
    }

    return RET_OK;
}

RET_CODE PushWork::DeInit()
{
    if(audio_capturer_) {
        audio_capturer_->Stop();
        delete audio_capturer_;
        audio_capturer_ = nullptr;
    }

    if(video_capturer_){
        video_capturer_->Stop();
        delete video_capturer_;
        video_capturer_ = nullptr;
    }
}

void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
    int ret = 0;

    int frame_bytes1 = 0;
    int frame_bytes2 = 0;
    // 默认读取出来的数据是s16的，编码器需要的是fltp, 需要做重采样
    // 手动把s16转成fltp
    frame_bytes1 = 1024 * 2 * 4;
    if (!fltp_buf_)
    {
        fltp_buf_ = new uint8_t[frame_bytes1];
    }
    
    int16_t *s16 = (int16_t *)pcm;
    float_t *fltp = (float_t *)fltp_buf_;
    for(int i = 0; i < 1024; i++) {
        // s16[0],s16[1];s16[2],s16[3];s16[4],s16[5];
        fltp[i]        =  s16[i*2 + 0]*1.0 / 32768; // 左通道  *2是因为我们默认现在是2通道  *1.0保证结果为float
        fltp[1024 + i] =  s16[i*2 + 1]*1.0 / 32768; // 右通道
    }

    AVFrame *audio_frame = av_frame_alloc();
    audio_frame->format = audio_encoder_->GetFormat();
    audio_frame->format = AV_SAMPLE_FMT_FLTP;
    audio_frame->nb_samples = audio_encoder_->GetFrameSamples();
    audio_frame->channels = audio_encoder_->GetChannels();
    audio_frame->channel_layout = audio_encoder_->GetChannelLayout();
    frame_bytes2  = audio_encoder_->GetFrameBytes();

    if (frame_bytes1 != frame_bytes2)
    {
        LogError("frame_bytes1 = %d, frame_bytes2 = %d", frame_bytes1, frame_bytes2);
        return;
    }
    
    if((ret = avcodec_fill_audio_frame(audio_frame, audio_encoder_->GetChannels(),
                                      (enum AVSampleFormat) audio_encoder_->GetFormat(), fltp_buf_, frame_bytes1, 0)) < 0)
    {
        // char buf[1024] = { 0 };
        // av_strerror(ret, buf, sizeof(buf) - 1);
        // LogError("Fail to avcodec_fill_audio_frame:%s", buf);
        LogError("avcodec_fill_audio_frame failed");
        return ;
    }

    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_audio_pts();
    int pkt_frame = 0;
    RET_CODE encode_ret = RET_OK;
    AVPacket *packet = audio_encoder_->Encode(audio_frame, pts, 0, &pkt_frame, &encode_ret);
    if(encode_ret == RET_OK && packet) 
    {
        if(!aac_fp_) 
        {
            aac_fp_ = fopen("push_dump.aac", "wb");
            if(!aac_fp_) 
            {
                LogError("fopen push_dump.aac failed");
                return;
            }
        }
        if(aac_fp_) 
        {
            uint8_t adts_header[7];
            if(audio_encoder_->GetAdtsHeader(adts_header, packet->size) != RET_OK) {
                LogError("GetAdtsHeader failed");
                return;
            }
            fwrite(adts_header, 1, 7, aac_fp_);
            fwrite(packet->data, 1, packet->size, aac_fp_);
        }
    }

    LogInfo("PcmCallback pts:%ld", pts);
    if(packet) {
        LogInfo("PcmCallback packet->pts:%ld", packet->pts);
        av_packet_free(&packet);
    }
    av_frame_free(&audio_frame);
}

void PushWork::YuvCallback(uint8_t *yuv, int32_t size)
{
    LogInfo("size:%d", size);
}