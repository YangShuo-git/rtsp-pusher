#include "rtsp_pusher.h"
#include "../../log/log.h"
#include "../../tools/times_util.h"

RtspPusher::RtspPusher(MessageQueue *msg_queue):msg_queue_(msg_queue)
{
    LogInfo("RtspPusher create");
}

RtspPusher::~RtspPusher()
{
    DeInit();  // 释放资源
}

// false:继续阻塞;  true:退出阻塞
static int decode_interrupt_cb(void *ctx)
{
    RtspPusher *rtsp_puser = (RtspPusher *)ctx;
    if(rtsp_puser->IsTimeout()) {
        LogWarn("timeout:%dms", rtsp_puser->GetTimeout());
        return 1;
    }
    // LogInfo("block time:%lld", rtsp_puser->GetBlockTime());
    return 0;
}


RET_CODE RtspPusher::Init(const Properties &properties)
{
    url_ = properties.GetProperty("url", "");
    rtsp_transport_ = properties.GetProperty("rtsp_transport", "");
    timeout_ = properties.GetProperty("timeout", 5000);
    audio_frame_duration_ = properties.GetProperty("audio_frame_duration", 0); 
    video_frame_duration_ = properties.GetProperty("video_frame_duration", 0);
    max_queue_duration_ = properties.GetProperty("max_queue_duration", 500);
    if(url_ == "") {
        LogError("url is nullptr");
        return RET_FAIL;
    }
    if(rtsp_transport_ == "") {
        LogError("rtsp_transport is nullptr, use udp or tcp");
        return RET_FAIL;
    }
    int ret = 0;
    char str_error[512] = {0};
    
    // 初始化网络库
    ret = avformat_network_init();
    if(ret < 0) {
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("Fail to avformat_network_init:%s", str_error);
        return RET_FAIL;
    }
    // 分配输出流的上下文 AVFormatContext
    ret = avformat_alloc_output_context2(&fmt_ctx_, nullptr, "rtsp", url_.c_str());
    if(ret < 0) {
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("Fail to avformat_alloc_output_context2:%s", str_error);
        return RET_FAIL;
    }
    ret = av_opt_set(fmt_ctx_->priv_data, "rtsp_transport", rtsp_transport_.c_str(), 0);
    if(ret < 0) {
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("Fail to av_opt_set:%s", str_error);
        return RET_FAIL;
    }

    // 设置rtsp连接超时回调（利用ffmpeg的参数）
    fmt_ctx_->interrupt_callback.callback = decode_interrupt_cb;    
    fmt_ctx_->interrupt_callback.opaque = this;

    // 创建packet队列
    queue_ = new PacketQueue(audio_frame_duration_, video_frame_duration_);
    if(!queue_) {
        LogError("Fail to new PacketQueue");
        return RET_ERR_OUTOFMEMORY;
    }

    return RET_OK;
}

void RtspPusher::DeInit()  // 这个函数重复调用没有问题
{
    if(queue_) {
        queue_->Abort();
    }
    Stop();
    if(fmt_ctx_) {
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    if(queue_) {
        delete queue_;
        queue_ = nullptr;
    }
}

RET_CODE RtspPusher::Push(AVPacket *pkt, MediaType media_type)
{
    int ret = queue_->Push(pkt, media_type);
    if(ret < 0) {
        return RET_FAIL;
    } else {
        return RET_OK;
    }
}

RET_CODE RtspPusher::Connect()
{
    if(!audio_stream_ && !video_stream_) {
        return RET_FAIL;
    }
    LogInfo("connect to:%s", url_.c_str());

    RestTiemout();
    // 连接服务器
    int ret = avformat_write_header(fmt_ctx_, nullptr);
    if(ret < 0) {
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("Fail to avformat_write_header:%s", str_error);
        return RET_FAIL;
    }
    LogInfo("avformat_write_header ok");

    // 启动推流线程
    return Start();     
}

void RtspPusher::Loop()
{
    LogInfo("Loop into: rtsp pusher");
    int ret = 0;
    AVPacket *pkt = nullptr;
    MediaType media_type;

    while (true) 
    {
        if(request_abort_) {
            LogInfo("abort request");
            break;
        }

        debugQueue(debug_interval_);
        checkPacketQueueDuration(); // 可以每隔一秒check一次
        ret = queue_->PopWithTimeout(&pkt, media_type, 1000);
        if(1 == ret) {  // 读取到消息
            if(request_abort_) {
                LogInfo("abort request");
                av_packet_free(&pkt);
                break;
            }
            switch (media_type) {
            case E_VIDEO_TYPE:
                ret = sendPacket(pkt, media_type);
                if(ret < 0) {
                    LogError("Fail to send video Packet");
                }
                av_packet_free(&pkt);
                break;
            case E_AUDIO_TYPE:
                ret = sendPacket(pkt, media_type);
                if(ret < 0) {
                    LogError("Fail to send audio Packet");
                }
                av_packet_free(&pkt);
                break;
            default:
                break;
            }
        }
    }
    
    RestTiemout();
    ret = av_write_trailer(fmt_ctx_);
    if(ret < 0) {
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("Fail to av_write_trailer:%s", str_error);
        return;
    }
    LogInfo("av_write_trailer ok");
}

int RtspPusher::sendPacket(AVPacket *pkt, MediaType media_type)
{
    AVRational dst_time_base;
    AVRational src_time_base = {1, 1000};  // 采集、编码的时间戳单位都是ms

    if(E_VIDEO_TYPE == media_type) {
        pkt->stream_index = video_index_;
        dst_time_base = video_stream_->time_base;
    } else if(E_AUDIO_TYPE == media_type) {
        pkt->stream_index = audio_index_;
        dst_time_base = audio_stream_->time_base;
    } else {
        LogError("unknown mediatype:%d", media_type);
        return -1;
    }

    pkt->pts = av_rescale_q(pkt->pts, src_time_base, dst_time_base);
    pkt->duration = 0;

    RestTiemout();
    int ret = av_write_frame(fmt_ctx_, pkt);
    if(ret < 0) {
        msg_queue_->notify_msg2(MSG_RTSP_ERROR, ret);
        char str_error[512] = {0};
        av_strerror(ret, str_error, sizeof(str_error) -1);
        LogError("Fail to av_write_frame :%s", str_error);
        return -1;
    }
    return 0;
}

RET_CODE RtspPusher::ConfigVideoStream(const AVCodecContext *ctx)
{
    if(!fmt_ctx_) {
        LogError("fmt_ctx is nullptr");
        return RET_FAIL;
    }
    if(!ctx) {
        LogError("ctx is nullptr");
        return RET_FAIL;
    }

    // 添加视频流
    AVStream *vs = avformat_new_stream(fmt_ctx_, nullptr);
    if(!vs) {
        LogError("Fail to avformat_new_stream: video");
        return RET_FAIL;
    }
    vs->codecpar->codec_tag = 0;

    // 从编码器拷贝信息
    avcodec_parameters_from_context(vs->codecpar, ctx);
    video_ctx_ = (AVCodecContext *) ctx;
    video_stream_ = vs;
    video_index_ = vs->index;   // 这个索引非常重要 fmt_ctx_根据index判别 音视频包
    return RET_OK;
}

RET_CODE RtspPusher::ConfigAudioStream(const AVCodecContext *ctx)
{
    if(!fmt_ctx_) {
        LogError("fmt_ctx is nullptr");
        return RET_FAIL;
    }
    if(!ctx) {
        LogError("ctx is nullptr");
        return RET_FAIL;
    }

    // 添加音频流
    AVStream *as = avformat_new_stream(fmt_ctx_, nullptr);
    if(!as) {
        LogError("Fail to avformat_new_stream: audio");
        return RET_FAIL;
    }
    as->codecpar->codec_tag = 0;

    // 从编码器拷贝信息
    avcodec_parameters_from_context(as->codecpar, ctx);
    audio_ctx_ = (AVCodecContext *) ctx;
    audio_stream_ = as;
    audio_index_ = as->index;
    return RET_OK;
}

bool RtspPusher::IsTimeout()
{
    if(TimesUtil::GetTimeMillisecond() - pre_time_ > timeout_) {
        return true;    // 超时
    }
    return false;
}

void RtspPusher::RestTiemout()
{
    pre_time_ = TimesUtil::GetTimeMillisecond();        // 重置为当前时间
}

int RtspPusher::GetTimeout()
{
    return timeout_;
}

int64_t RtspPusher::GetBlockTime()
{
    return TimesUtil::GetTimeMillisecond() - pre_time_;
}

void RtspPusher::debugQueue(int64_t interval)
{
    int64_t cur_time = TimesUtil::GetTimeMillisecond();
    if(cur_time - pre_debug_time_ > interval) {
        // 打印信息
        PacketQueueStats stats;
        queue_->GetStats(&stats);
        LogInfo("duration:a-%lldms, v-%lldms", stats.audio_duration, stats.video_duration);
        pre_debug_time_ = cur_time;
    }
}

void RtspPusher::checkPacketQueueDuration()
{
    PacketQueueStats stats;
    queue_->GetStats(&stats);
    if(stats.audio_duration > max_queue_duration_ || stats.video_duration > max_queue_duration_) 
    {
        msg_queue_->notify_msg3(MSG_RTSP_QUEUE_DURATION, stats.audio_duration, stats.video_duration);
        LogWarn("drop packet -> a:%lld, v:%lld, th:%d", stats.audio_duration, stats.video_duration,
                max_queue_duration_);
        queue_->Drop(false, max_queue_duration_);
    }
}