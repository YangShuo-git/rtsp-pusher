#include "aac_encoder.h"
#include "../../log/log.h"


AACEncoder::AACEncoder()
{
}

AACEncoder::~AACEncoder()
{
    if(codecCtx_) 
    {
        avcodec_free_context(&codecCtx_);
    }
}

RET_CODE AACEncoder::Init(const Properties &properties)
{
    // 配置编码器所需的参数
    sample_rate_ = properties.GetProperty("sample_rate", 48000);
    channels_ = properties.GetProperty("channels", 2);
    bitrate_  = properties.GetProperty("bitrate", 128*1024);
    channel_layout_ = properties.GetProperty("channel_layout",
                                             (int)av_get_default_channel_layout(channels_));

    // 查找编码器
    codec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if(!codec_) 
    {
        LogError("Fail to find audio encoder");
        return RET_ERR_MISMATCH_CODE;
    }
    // 分配编码器上下文
    codecCtx_ = avcodec_alloc_context3(codec_);
    if(!codecCtx_) 
    {
        LogError("Fail to avcodec_alloc_context3: audio");
        return RET_ERR_OUTOFMEMORY;
    }

    // 给编码器配置参数（使用的是之前设置的参数）
    codecCtx_->sample_rate   = sample_rate_;
    codecCtx_->sample_fmt    = AV_SAMPLE_FMT_FLTP;  // 这里默认是aac编码：planar格式PCM， 如果是fdk-aac，会不一样
    codecCtx_->channels      = channels_;
    codecCtx_->channel_layout= channel_layout_;
    codecCtx_->bit_rate      = bitrate_;
    // Allow experimental codecs
    codecCtx_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    // 打开编码器
    if(avcodec_open2(codecCtx_, codec_, nullptr) < 0) 
    {
        LogError("Fail to avcodec_open2: audio");
        avcodec_free_context(&codecCtx_);
        return RET_FAIL;
    }

    return RET_OK;
}

AVPacket *AACEncoder::Encode(AVFrame *frame, const int64_t pts, int flush, int *pkt_frame, RET_CODE *ret)
{
    int ret1 = 0;
    *pkt_frame = 0;
    if(!codecCtx_) 
    {
        *ret = RET_FAIL;
        LogError("AAC: no context");
        return nullptr;
    }
    if(frame) 
    {
        frame->pts = pts;
        ret1 = avcodec_send_frame(codecCtx_, frame);
        //        av_frame_unref(frame);
        if(ret1 < 0)  // 小于0 不能正常处理该frame
        {  
            char buf[1024] = { 0 };
            av_strerror(ret1, buf, sizeof(buf) - 1);
            LogError("Fail to avcodec_send_frame:%s", buf);
            *pkt_frame = 1;
            if(ret1 == AVERROR(EAGAIN))  // 你赶紧读取packet，我frame send不进去了
            {      
                *ret = RET_ERR_EAGAIN;
                return nullptr;
            } else if(ret1 == AVERROR_EOF) 
            {
                *ret = RET_ERR_EOF;
                return nullptr;
            } else 
            {
                *ret = RET_FAIL;            // 真正报错，这个encoder就只能销毁了
                return nullptr;
            }
        }
    }
    if(flush)  // 只能调用一次 
    {     
        avcodec_flush_buffers(codecCtx_);
    }

    AVPacket *packet = av_packet_alloc();
    ret1 = avcodec_receive_packet(codecCtx_, packet);
    if(ret1 < 0) 
    {
        LogError("AAC: avcodec_receive_packet ret:%d", ret1);
        av_packet_free(&packet);
        *pkt_frame = 0;
        if(ret1 == AVERROR(EAGAIN))  // 需要继续发送frame，我们才有packet读取
        {       
            *ret = RET_ERR_EAGAIN;
            return nullptr;
        }else if(ret1 == AVERROR_EOF)  // 不能再读取出来packet来了
        {
            *ret = RET_ERR_EOF;            
            return nullptr;
        } else  
        {
            *ret = RET_FAIL; // 真正报错，这个encoder就只能销毁了       
            return nullptr;
        }
    }else {
        *ret = RET_OK;
        return packet;
    }
}

RET_CODE AACEncoder::GetAdtsHeader(uint8_t *adts_header, int aac_length)
{
    uint8_t freqIdx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (codecCtx_->sample_rate)
    {
    case 96000: freqIdx = 0; break;
    case 88200: freqIdx = 1; break;
    case 64000: freqIdx = 2; break;
    case 48000: freqIdx = 3; break;
    case 44100: freqIdx = 4; break;
    case 32000: freqIdx = 5; break;
    case 24000: freqIdx = 6; break;
    case 22050: freqIdx = 7; break;
    case 16000: freqIdx = 8; break;
    case 12000: freqIdx = 9; break;
    case 11025: freqIdx = 10; break;
    case 8000: freqIdx = 11; break;
    case 7350: freqIdx = 12; break;
    default:
        LogError("Can't support sample_rate:%d");
        freqIdx = 4;
        return RET_FAIL;
    }
    uint8_t ch_cfg = codecCtx_->channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((codecCtx_->profile) << 6) + (freqIdx << 2) + (ch_cfg >> 2);
    adts_header[3] = (((ch_cfg & 3) << 6) + (frame_length  >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
    return RET_OK;
}