#include <functional>
#include "log.h"
#include "push_work.h"
#include "avpublish_time.h"

PushWork::PushWork()
{

}

PushWork::~PushWork()
{
    // 从源头开始释放资源
    // 先释放音频、视频捕获
    if(audio_capturer_) {
        delete audio_capturer_;
    }
    if(video_capturer_) {
        delete video_capturer_;
    }
    if(audio_encoder_) {
        delete audio_encoder_;
    }
    if(fltp_buf_) {
        av_free(fltp_buf_);
    }
    if(audio_frame_) {
        av_frame_free(&audio_frame_);
    }
    if(video_encoder_) {
        delete video_encoder_;
    }
    LogInfo("~PushWork()");
}

RET_CODE PushWork::Init(const Properties &properties)
{
    int ret = 0;
    
    // 文件模式（代替物理采集）
    audio_test_ = properties.GetProperty("audio_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");
    video_test_ = properties.GetProperty("video_test", 0);
    input_yuv_name_ = properties.GetProperty("input_yuv_name", "input_1280_720_420p.yuv");

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

    // 音频编码参数
    audio_sample_rate_ = properties.GetProperty("audio_sample_rate", mic_sample_rate_);
    audio_bitrate_ = properties.GetProperty("audio_bitrate", 128*1024);
    audio_channels_ = properties.GetProperty("audio_channels", mic_channels_);
    audio_ch_layout_ = av_get_default_channel_layout(audio_channels_);    // 由audio_channels_决定

    // 视频编码属性
    video_width_  = properties.GetProperty("video_width", desktop_width_); 
    video_height_ = properties.GetProperty("video_height", desktop_height_); 
    video_fps_ = properties.GetProperty("video_fps", desktop_fps_);             
    video_gop_ = properties.GetProperty("video_gop", video_fps_);
    video_bitrate_ = properties.GetProperty("video_bitrate", 1024*1024);   // 先默认1M fixedme
    video_b_frames_ = properties.GetProperty("video_b_frames", 0); 

    // 初始化publish time
    AVPublishTime::GetInstance()->Rest();   // 推流打时间戳的问题


    // 设置音频编码器（通过上面采集到的参数来进行设置）
    audio_encoder_ = new AACEncoder();
    if(!audio_encoder_)
    {
        LogError("Fail to new AACEncoder");
        return RET_FAIL;
    }
    Properties  aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate", audio_sample_rate_);
    aud_codec_properties.SetProperty("channels", audio_channels_);
    aud_codec_properties.SetProperty("bitrate", audio_bitrate_);
    // 这里的属性没有去设置采样格式  因为采样格式是从new出来的编码器读取出来的
    if(audio_encoder_->Init(aud_codec_properties) != RET_OK)
    {
        LogError("Fail to AACEncoder Init");
        return RET_FAIL;
    }

    // 设置视频编码器
    video_encoder_ = new H264Encoder();
    Properties vid_codec_properties;
    vid_codec_properties.SetProperty("width", video_width_);
    vid_codec_properties.SetProperty("height", video_height_);
    vid_codec_properties.SetProperty("fps", video_fps_);            
    vid_codec_properties.SetProperty("b_frames", video_b_frames_);
    vid_codec_properties.SetProperty("bitrate", video_bitrate_); 
    vid_codec_properties.SetProperty("gop", video_gop_);
    if(video_encoder_->Init(vid_codec_properties) != RET_OK)
    {
        LogError("Fail to Init H264Encoder");
        return RET_FAIL;
    }


    // 音频重采样 默认读取出来的数据是s16的，编码器需要的是fltp,
    // 手动把s16转成fltp
    int frame_bytes2 = 0;
    frame_bytes2 = audio_encoder_->GetFrameBytes(); // s16格式 一帧的字节数
    fltp_buf_size_ = av_samples_get_buffer_size(NULL, audio_encoder_->GetChannels(),  
                                              audio_encoder_->GetFrameSamples(),
                                              (enum AVSampleFormat)audio_encoder_->GetFormat(), 1);  // fltp格式 一帧的字节数
    fltp_buf_ = (uint8_t *)av_malloc(fltp_buf_size_);
    if(!fltp_buf_) {
        LogError("Fail to fltp_buf_ av_malloc");
        return RET_ERR_OUTOFMEMORY;
    }

    audio_frame_ = av_frame_alloc();
    audio_frame_->format = audio_encoder_->GetFormat();
    audio_frame_->nb_samples = audio_encoder_->GetFrameSamples();
    audio_frame_->channels = audio_encoder_->GetChannels();
    audio_frame_->channel_layout = audio_encoder_->GetChannelLayout();
    if(fltp_buf_size_ != frame_bytes2) {
        LogError("fltp_buf_size_:%d != frame_bytes2:%d", fltp_buf_size_, frame_bytes2);
        return RET_FAIL;
    }

    ret = av_frame_get_buffer(audio_frame_, 0);
    if(ret < 0) {
        LogError("Fail to av_frame_get_buffer ");
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
        LogError("Fail to Init AudioCapturer");
        return RET_FAIL;
    }
    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this, 
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    if(audio_capturer_->Start()!= RET_OK) 
    {
         LogError("Fail to Start AudioCapturer");
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
        LogError("Fail to Init VideoCapturer");
        return RET_FAIL;
    }
//    video_nalu_buf = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];
    video_capturer_->AddCallback(std::bind(&PushWork::YuvCallback, this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    if(video_capturer_->Start()!= RET_OK) 
    {
        LogError("Fail to Start VideoCapturer");
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

// s16交错模式 转为 float planar格式  只支持2通道 
void s16le_convert_to_fltp(short *s16le, float *fltp, int nb_samples) {
    float *fltp_l = fltp;   // -1~1
    float *fltp_r = fltp + nb_samples;  // 要加一帧的采样点，才会变为右
    for(int i = 0; i < nb_samples; i++) {
        fltp_l[i] = s16le[i*2]/32768.0;     // 0 2 4 表示偶数
        fltp_r[i] = s16le[i*2+1]/32768.0;   // 1 3 5 表示奇数
    }
}

void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
    int ret = 0;
    if(!pcm_s16le_fp_)
    {
        pcm_s16le_fp_ = fopen("build/push_dump_s16le.pcm", "wb");
    }
    if(pcm_s16le_fp_)
    {
        // ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
        fwrite(pcm, 1, size, pcm_s16le_fp_);
        fflush(pcm_s16le_fp_);
    }
    // 这里就约定好，音频捕获的时候，一帧的采样点和编码器需要的点数是一样的
    s16le_convert_to_fltp((short *)pcm, (float *)fltp_buf_, audio_frame_->nb_samples);
    ret = av_frame_make_writable(audio_frame_);
    if(ret < 0) {
        LogError("Fail to av_frame_make_writable");
        return;
    }
    // 将fltp_buf_写入frame
    ret = av_samples_fill_arrays(audio_frame_->data,
                                 audio_frame_->linesize,
                                 fltp_buf_,
                                 audio_frame_->channels,
                                 audio_frame_->nb_samples,
                                 (AVSampleFormat)audio_frame_->format,
                                 0);
    if(ret < 0) {
        LogError("Fail to av_samples_fill_arrays");
        return;
    }

    // 打上时间戳
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_audio_pts();
    int pkt_frame = 0;
    RET_CODE encode_ret = RET_OK;
    AVPacket *packet = audio_encoder_->Encode(audio_frame_, pts, 0, &pkt_frame, &encode_ret);
    if(encode_ret == RET_OK && packet) 
    {
        if(!aac_fp_) 
        {
            aac_fp_ = fopen("build/push_dump.aac", "wb");
            if(!aac_fp_) 
            {
                LogError("Fail to fopen push_dump.aac");
                return;
            }
        }
        if(aac_fp_) 
        {
            uint8_t adts_header[7];
            if(audio_encoder_->GetAdtsHeader(adts_header, packet->size) != RET_OK) {
                LogError("Fail to GetAdtsHeader");
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
    }else
    {
        LogInfo("packet is null");
    }
}

void PushWork::YuvCallback(uint8_t *yuv, int32_t size)
{
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_video_pts();
    int pkt_frame = 0;
    RET_CODE encode_ret = RET_OK;
    AVPacket *packet = video_encoder_->Encode(yuv, size, pts, &pkt_frame, &encode_ret);
    if(packet) {
        if(!h264_fp_) 
        {
            h264_fp_ = fopen("build/push_dump.h264", "wb");
            if(!h264_fp_) 
            {
                LogError("Fail to fopen push_dump.h264");
                return;
            }
            // 写入sps 和pps
            uint8_t start_code[] = {0, 0, 0, 1};
            fwrite(start_code, 1, 4, h264_fp_);
            fwrite(video_encoder_->get_sps_data(), 1, video_encoder_->get_sps_size(), h264_fp_);
            fwrite(start_code, 1, 4, h264_fp_);
            fwrite(video_encoder_->get_pps_data(), 1, video_encoder_->get_pps_size(), h264_fp_);
        }

        fwrite(packet->data, 1,  packet->size, h264_fp_);

        fflush(h264_fp_);
    }
    LogInfo("YuvCallback pts:%ld", pts);
    if(packet) {
        LogInfo("YuvCallback packet->pts:%ld", packet->pts);
        av_packet_free(&packet);
    }else {
        LogInfo("packet is null");
    }
}