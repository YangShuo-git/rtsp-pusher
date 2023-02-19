#ifndef _COMMON_LOOP_H_
#define _COMMON_LOOP_H_

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
    virtual void Loop() = 0;

private:
    static void* trampoline(void *p);

protected:
};

CommonLooper::CommonLooper(/* args */)
{
}

CommonLooper::~CommonLooper()
{
}


#endif // _COMMON_LOOP_H_