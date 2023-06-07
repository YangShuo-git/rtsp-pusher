#include <functional>
#include "../log/log.h"
#include "push_work.h"
#include "../tools/avpublish_time.h"

PushWork::PushWork(MessageQueue *msg_queue):msg_queue_(msg_queue)
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
    if (rtsp_pusher_)
    {
        delete rtsp_pusher_;
    }
    LogInfo("~PushWork()");
}

RET_CODE PushWork::Init(const Properties &properties)
{
    int ret = 0;
    
    // 文件模式（代替物理采集）
    audio_test_ = properties.GetProperty("audio_test", 0);
    video_test_ = properties.GetProperty("video_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");
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
    audio_channels_ = properties.GetProperty("audio_channels", mic_channels_);
    audio_bitrate_ = properties.GetProperty("audio_bitrate", 128*1024);
    audio_ch_layout_ = av_get_default_channel_layout(audio_channels_);    // 由audio_channels_决定

    // 视频编码参数
    video_width_  = properties.GetProperty("video_width", 720); 
    video_height_ = properties.GetProperty("video_height", 480); 
    video_fps_ = properties.GetProperty("video_fps", 25);             
    video_gop_ = properties.GetProperty("video_gop", video_fps_);
    video_bitrate_ = properties.GetProperty("video_bitrate", 1024*1024);   // 先默认1M fixedme
    video_b_frames_ = properties.GetProperty("video_b_frames", 0); 

    // rtsp参数配置
    rtsp_url_       = properties.GetProperty("rtsp_url", "");
    rtsp_transport_ = properties.GetProperty("rtsp_transport", "");
    rtsp_timeout_ = properties.GetProperty("rtsp_timeout", 5000);
    rtsp_max_queue_duration_ = properties.GetProperty("rtsp_max_queue_duration", 500);

    // 初始化publish time
    AVPublishTime::GetInstance()->Rest();  // 推流打时间戳的问题


    // 设置音频编码器（通过上面获取到的参数来进行设置）
    audio_encoder_ = new AACEncoder();
    if(!audio_encoder_){
        LogError("Fail to new AACEncoder");
        return RET_FAIL;
    }
    Properties aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate", audio_sample_rate_);
    aud_codec_properties.SetProperty("channels", audio_channels_);
    aud_codec_properties.SetProperty("bitrate", audio_bitrate_);
    // 这里的属性没有去设置采样格式  因为采样格式是从new出来的编码器读取出来的
    if(audio_encoder_->Init(aud_codec_properties) != RET_OK)
    {
        LogError("Fail to Init AACEncoder");
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
    if(video_encoder_->Init(vid_codec_properties) != RET_OK){
        LogError("Fail to Init H264Encoder");
        return RET_FAIL;
    }


    // 音频重采样准备 默认读取出来的数据是s16的，编码器需要的是fltp；这里是手动把s16转成fltp，且两者每帧的字节数要保持一致
    int frame_bytes = 0;
    frame_bytes = audio_encoder_->GetFrameBytes(); // s16格式 一帧的字节数
    fltp_buf_size_ = av_samples_get_buffer_size(NULL, audio_encoder_->GetChannels(),  
                                              audio_encoder_->GetFrameSamples(),
                                              (enum AVSampleFormat)audio_encoder_->GetFormat(), 1);  // fltp格式 一帧的字节数
    fltp_buf_ = (uint8_t *)av_malloc(fltp_buf_size_);
    if(!fltp_buf_) {
        LogError("Fail to fltp_buf_ av_malloc");
        return RET_ERR_OUTOFMEMORY;
    }

    audio_frame_ = av_frame_alloc();
    audio_frame_->nb_samples = audio_encoder_->GetFrameSamples();
    audio_frame_->format     = audio_encoder_->GetFormat();
    audio_frame_->channels   = audio_encoder_->GetChannels();
    audio_frame_->channel_layout = audio_encoder_->GetChannelLayout();
    if(fltp_buf_size_ != frame_bytes) {
        LogError("fltp_buf_size_:%d != frame_bytes:%d", fltp_buf_size_, frame_bytes);
        return RET_FAIL;
    }

    ret = av_frame_get_buffer(audio_frame_, 0);
    if(ret < 0) {
        LogError("Fail to av_frame_get_buffer ");
        return RET_FAIL;
    }


    // 设置 Rtsp Pusher（初始化、创建新流、网络连接） 在音视频编码器初始化后，音视频捕获前
    rtsp_pusher_ = new RtspPusher(msg_queue_);
    if(!rtsp_pusher_) {
        LogError("Fail to new RTSPPusher()");
        return RET_FAIL;
    }
    Properties rtsp_properties;
    rtsp_properties.SetProperty("url", rtsp_url_);
    rtsp_properties.SetProperty("rtsp_transport", rtsp_transport_);
    rtsp_properties.SetProperty("timeout", rtsp_timeout_);
    rtsp_properties.SetProperty("max_queue_duration", rtsp_max_queue_duration_);
    if (audio_encoder_) {
        rtsp_properties.SetProperty("audio_frame_duration", 
        audio_encoder_->GetFrameSamples()*1000/audio_encoder_->GetSampleRate());
    }
    if (video_encoder_) {
        rtsp_properties.SetProperty("video_frame_duration", 1000/video_encoder_->GetFps());
    }
    // 初始化
    if(rtsp_pusher_->Init(rtsp_properties) != RET_OK) {
        LogError("Fail to rtsp_pusher_->Init");
        return RET_FAIL;
    }

    // 创建视频流、音频流
    if(video_encoder_) {
        if(rtsp_pusher_->ConfigVideoStream(video_encoder_->GetCodecContext()) != RET_OK) {
            LogError("Fail to rtsp_pusher ConfigVideoSteam");
            return RET_FAIL;
        }
    }
    if(audio_encoder_) {
        if(rtsp_pusher_->ConfigAudioStream(audio_encoder_->GetCodecContext()) != RET_OK) {
            LogError("Fail to rtsp_pusher ConfigAudioStream");
            return RET_FAIL;
        }
    }

    // 网络连接  会启动推流线程
    if(rtsp_pusher_->Connect() != RET_OK) {
        LogError("Fail to rtsp_pusher Connect()");
        return RET_FAIL;
    }


    // 设置音频捕获
    audio_capturer_ = new AudioCapturer();
    Properties aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test", 1);
    aud_cap_properties.SetProperty("input_pcm_name", input_pcm_name_);
    aud_cap_properties.SetProperty("sample_rate", audio_sample_rate_);
    aud_cap_properties.SetProperty("format", AV_SAMPLE_FMT_S16);
    aud_cap_properties.SetProperty("channels", audio_channels_);
    aud_cap_properties.SetProperty("nb_samples", 1024);     // 由编码器提供
    aud_cap_properties.SetProperty("byte_per_sample", 2);    
    if(audio_capturer_->Init(aud_cap_properties) != RET_OK)
    {
        LogError("Fail to Init AudioCapturer");
        return RET_FAIL;
    }
    // PcmCallback是PushWork中的函数，bind成回调函数，并注册到audio_capturer_中
    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this, 
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    // 启动音频采集线程
    if(audio_capturer_->startThread()!= RET_OK)
    {
         LogError("Fail to startThread AudioCapturer");
        return RET_FAIL;
    }

    // 设置视频捕获
    video_capturer_ = new VideoCapturer();
    Properties vid_cap_properties;
    vid_cap_properties.SetProperty("video_test", 1);
    vid_cap_properties.SetProperty("input_yuv_name", input_yuv_name_);
    vid_cap_properties.SetProperty("width", video_width_);
    vid_cap_properties.SetProperty("height", video_height_);
    if(video_capturer_->Init(vid_cap_properties) != RET_OK)
    {
        LogError("Fail to Init VideoCapturer");
        return RET_FAIL;
    }
    // video_nalu_buf = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];
    video_capturer_->AddCallback(std::bind(&PushWork::YuvCallback, this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    if(video_capturer_->startThread()!= RET_OK) 
    {
        LogError("Fail to startThread VideoCapturer");
        return RET_FAIL;
    }

    return RET_OK;
}

RET_CODE PushWork::DeInit()
{
    if(audio_capturer_) {
        audio_capturer_->stopThread();
        delete audio_capturer_;
        audio_capturer_ = nullptr;
    }

    if(video_capturer_){
        video_capturer_->stopThread();
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
    int pkt_frame = 0;
    RET_CODE encode_ret = RET_OK;

    // 将采集到的pcm包输出到一个新的pcm文件 push_dump_s16le.pcm
    if(!pcm_s16le_fp_)
    {
        pcm_s16le_fp_ = fopen("build/dump_s16le.pcm", "wb");
    }
    if(pcm_s16le_fp_)
    {
        // ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
        fwrite(pcm, 1, size, pcm_s16le_fp_);
        fflush(pcm_s16le_fp_);
    }


    // 将frame编码为packet前的准备工作
    // 将采样数据从16位有符号整数格式（s16le）转换为浮点数格式（fltp）
    // 这里就约定好一帧的采样点，音频捕获需要的和编码器需要的点数是一样的
    s16le_convert_to_fltp((short *)pcm, (float *)fltp_buf_, audio_frame_->nb_samples);
    // 确保audio_frame_的缓冲区可写，并且可以对其进行修改或填充
    ret = av_frame_make_writable(audio_frame_);
    if(ret < 0) {
        LogError("Fail to av_frame_make_writable");
        return;
    }
    // 将源缓冲区 fltp_buf_ 中的音频数据填充到目标数据数组audio_frame_->data中
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

    // 编码操作
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_audio_pts();  // 打上时间戳
    AVPacket *packet = audio_encoder_->Encode(audio_frame_, pts, 0, &pkt_frame, &encode_ret);

    // 将编码后的packet输出为aac文件 push_dump.aac  为了保证播放，需要加上7Byte adts header
    if(encode_ret == RET_OK && packet) 
    {
        if(!aac_fp_) 
        {
            aac_fp_ = fopen("build/dump.aac", "wb");
            if(!aac_fp_) 
            {
                LogError("Fail to fopen dump.aac");
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

    // 将编码后的packet push到推流队列
    // LogInfo("PcmCallback pts:%ld", pts);
    if(packet) {
        // LogInfo("PcmCallback packet->pts:%ld", packet->pts);
        // av_packet_free(&packet);  要push到推流队列中，所以不能free了
        rtsp_pusher_->Push(packet, E_AUDIO_TYPE);
    }else {
        av_packet_free(&packet);
        LogInfo("packet is null");
    }
}

void PushWork::YuvCallback(uint8_t *yuv, int32_t size)
{
    int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_video_pts();
    int pkt_frame = 0;
    RET_CODE encode_ret = RET_OK;
    AVPacket *packet = video_encoder_->Encode(yuv, size, pts, &pkt_frame, &encode_ret);
    if(packet) 
    {
        if(!h264_fp_) 
        {
            h264_fp_ = fopen("build/dump.h264", "wb");
            if(!h264_fp_) 
            {
                LogError("Fail to fopen dump.h264");
                return;
            }
            // 写入sps 和pps
            uint8_t start_code[] = {0, 0, 0, 1};
            fwrite(start_code, 1, 4, h264_fp_);
            fwrite(video_encoder_->get_sps_data(), 1, video_encoder_->get_sps_size(), h264_fp_);
            fwrite(start_code, 1, 4, h264_fp_);
            fwrite(video_encoder_->get_pps_data(), 1, video_encoder_->get_pps_size(), h264_fp_);
        }

        fwrite(packet->data, 1, packet->size, h264_fp_);

        fflush(h264_fp_);
    }

    // LogInfo("YuvCallback pts:%ld", pts);
    if(packet) 
    {
        // LogInfo("YuvCallback packet->pts:%ld", packet->pts);
        // av_packet_free(&packet);
        rtsp_pusher_->Push(packet, E_VIDEO_TYPE);
    }else {
        LogInfo("packet is null");
    }
}