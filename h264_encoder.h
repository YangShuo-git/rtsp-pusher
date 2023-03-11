#ifndef _H264_ENCODER_H_
#define _H264_ENCODER_H_

#include "media_base.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

class H264Encoder
{
public:
    H264Encoder();
    virtual ~H264Encoder();
    
    virtual int Init(const Properties &properties);
    // pkt_frame 用来定位报错的位置  
    // 若*pkt_frame = 1, 表示send_frame报错；若*pkt_frame = 0，表示receive_packet报错;
    virtual AVPacket *Encode(uint8_t *yuv, int size, int64_t pts, int *pkt_frame, RET_CODE *ret);

    inline uint8_t *get_sps_data()
    {
        return (uint8_t *)sps_.c_str();
    }
    inline int get_sps_size()
    {
        return sps_.size();
    }
    inline uint8_t *get_pps_data()
    {
        return (uint8_t *)pps_.c_str();
    }
    inline int get_pps_size()
    {
        return pps_.size();
    }
    AVCodecContext *GetCodecContext() 
    {
        return ctx_;
    }

private:
    int width_ = 0;
    int height_ = 0;
    int fps_ = 0;        // 帧率
    int b_frames_ = 0;   // 连续B帧的数量  有b帧，一定有延迟
    int bitrate_ = 0;    // 码率  越高，越清晰
    int gop_ = 0;        // 多少帧有一个I帧  影响秒开；帧率为25，则gop是50
    int pix_fmt_ = 0;    // 像素格式
    bool annexb_  = false;  // 默认不带起始码
    int threads_ = 1;
    //    std::string profile_;
    //    std::string level_id_;

    std::string sps_;
    std::string pps_;
    std::string codec_name_;
    AVCodec *codec_ = nullptr;
    AVCodecContext  *ctx_ = nullptr;
    AVDictionary *dict_ = nullptr;  // 有些参数需要字典去传递

    AVFrame *frame_ = nullptr;
};

#endif // _H264_ENCODER_H_