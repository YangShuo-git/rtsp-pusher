#ifndef _COMMON_LOOPER_H_
#define _COMMON_LOOPER_H_

#include <thread>
#include "media_base.h"

class CommonLooper
{
private:
    /* data */
public:
    CommonLooper(/* args */);
    virtual ~CommonLooper();

    virtual RET_CODE Start();  // 开启线程
    virtual void Stop();  // 停止线程
    virtual bool Running();
    virtual void SetRunning(bool running);
    virtual void Loop() = 0;

private:
    static void* trampoline(void *p);

protected:
    std::thread *worker_ = nullptr;  // 线程
    bool request_abort_ = false;  // 请求退出线程的标志
    bool running_ =  true;  // 线程是否在运行
};

#endif // _COMMON_LOOPER_H_
