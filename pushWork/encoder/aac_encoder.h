#ifndef _AAC_ENCODER_H_
#define _AAC_ENCODER_H_

#include "../media_base.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class AACEncoder
{
public:
    AACEncoder();
    virtual ~AACEncoder();

    /**
     * "sample_rate"     采样率，默认48000
     * "channels"        通道数量 ，默认2
     * "channel_layout"  通道布局，默认根据channels获取默认的
     * "bitrate"         比特率，默认128*1024
     */
    RET_CODE Init(const Properties &properties);

    /**
     * frame     输入帧
     * pts       时间戳
     * flush     是否flush
     * pkt_frame 若*pkt_frame = 1, 表示send_frame报错；若*pkt_frame = 0，表示receive_packet报错;
     * ret       只有RET_OK才不需要做异常处理
     */
    virtual AVPacket *Encode(AVFrame *frame, const int64_t pts, int flush, int *pkt_frame, RET_CODE *ret);

    RET_CODE GetAdtsHeader(uint8_t *adts_header, int aac_length);

    virtual int GetSampleRate() 
    {
        return codecCtx_->sample_rate;
    }
    virtual int GetFormat() 
    {
        return codecCtx_->sample_fmt;
    }
    virtual int GetChannels() 
    {
        return codecCtx_->channels;
    }
    virtual int GetChannelLayout() 
    {
        return codecCtx_->channel_layout;
    }
    // 一帧有多少个采样点 (采样点数量，只是说的一个通道)
    virtual int GetFrameSamples() 
    {      
        return codecCtx_->frame_size;
    }
    // 一帧占用的字节数
    virtual int GetFrameBytes() 
    {
        return av_get_bytes_per_sample(codecCtx_->sample_fmt) * codecCtx_->channels * codecCtx_->frame_size;
    }
    AVCodecContext *GetCodecContext() 
    {
        return codecCtx_;
    }

//    virtual RET_CODE EncodeInput(const AVFrame *frame);
//    virtual RET_CODE EncodeOutput(AVPacket *pkt);

private:
    // C++11可以直接初始化，不必在构造函数中
    int sample_rate_ = 48000; 
    int channels_    = 2;
    int bitrate_     = 128*1024;
    int channel_layout_ = AV_CH_LAYOUT_STEREO;
    std::string codec_name_;

    AVCodec *codec_        = nullptr;  // 编码器
    AVCodecContext  *codecCtx_  = nullptr;  // 编码器上下文
};




#endif // _AAC_ENCODER_H_